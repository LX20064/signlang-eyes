#include "signlang_model.hpp"

#include "spdlog/spdlog.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace signlang::signlang_det {
  namespace {

    constexpr auto kPrototypeMagic = std::array<char, 8>{'S', 'L', 'D', 'T', 'W', 'P', 'B', '\0'};
    constexpr auto kPrototypeVersion = std::uint32_t{1};

    template <typename T>
    void read_binary(std::ifstream& file, T& value, const char* field_name) {
      file.read(reinterpret_cast<char*>(&value), sizeof(T));
      if (!file) {
        throw std::runtime_error(std::string{"Failed to read prototype field: "} + field_name);
      }
    }

    auto trim(std::string value) -> std::string {
      const auto first = value.find_first_not_of(" \t\r\n");
      if (first == std::string::npos) {
        return {};
      }
      const auto last = value.find_last_not_of(" \t\r\n");
      return value.substr(first, last - first + 1);
    }

    auto parse_label_line(const std::string& line, std::uint32_t fallback_id) -> GestureLabel {
      auto stream = std::istringstream{line};
      auto first = std::string{};
      stream >> first;
      if (first.empty()) {
        throw std::runtime_error("Empty gesture label line");
      }

      try {
        std::size_t parsed_chars = 0;
        const auto parsed_id = static_cast<std::uint32_t>(std::stoul(first, &parsed_chars, 10));
        if (parsed_chars == first.size()) {
          auto name = std::string{};
          std::getline(stream, name);
          name = trim(name);
          if (name.empty()) {
            throw std::runtime_error("Gesture label id is missing a name: " + line);
          }
          return GestureLabel{.id = parsed_id, .name = std::move(name)};
        }
      } catch (const std::invalid_argument&) {
      } catch (const std::out_of_range&) {
        throw std::runtime_error("Gesture label id is out of range: " + line);
      }

      return GestureLabel{.id = fallback_id, .name = trim(line)};
    }

    class RknnOutputReleaseGuard {
    public:
      RknnOutputReleaseGuard(rknn_context context, std::uint32_t output_count, rknn_output* outputs)
        : context_{context}, output_count_{output_count}, outputs_{outputs} {}

      RknnOutputReleaseGuard(const RknnOutputReleaseGuard&) = delete;
      auto operator=(const RknnOutputReleaseGuard&) -> RknnOutputReleaseGuard& = delete;
      RknnOutputReleaseGuard(RknnOutputReleaseGuard&&) = delete;
      auto operator=(RknnOutputReleaseGuard&&) -> RknnOutputReleaseGuard& = delete;

      ~RknnOutputReleaseGuard() {
        static_cast<void>(rknn_outputs_release(context_, output_count_, outputs_));
      }

    private:
      rknn_context context_;
      std::uint32_t output_count_;
      rknn_output* outputs_;
    };

  } // namespace

  auto GestureLabelMap::name_for(std::uint32_t gesture_id) const -> const char* {
    if (const auto found = names_by_id.find(gesture_id); found != names_by_id.end()) {
      return found->second.c_str();
    }
    return "unknown";
  }

  auto load_gesture_labels(const std::string& path) -> GestureLabelMap {
    auto file = std::ifstream{path};
    if (!file.is_open()) {
      throw std::runtime_error("Failed to open label map file: " + path);
    }

    auto label_map = GestureLabelMap{};
    auto line = std::string{};
    auto fallback_id = std::uint32_t{0};
    while (std::getline(file, line)) {
      line = trim(line);
      if (line.empty() || line.front() == '#') {
        continue;
      }

      auto label = parse_label_line(line, fallback_id);
      if (label_map.names_by_id.find(label.id) != label_map.names_by_id.end()) {
        throw std::runtime_error("Duplicate gesture id in label map: " + std::to_string(label.id));
      }
      label_map.names_by_id.emplace(label.id, label.name);
      label_map.labels.push_back(std::move(label));
      ++fallback_id;
    }

    if (label_map.labels.empty()) {
      throw std::runtime_error("Label map file is empty: " + path);
    }

    return label_map;
  }

  auto PrototypeStore::load(const std::string& path) -> PrototypeStore {
    auto file = std::ifstream{path, std::ios::binary};
    if (!file.is_open()) {
      throw std::runtime_error("Failed to open prototypes file: " + path);
    }

    auto store = PrototypeStore{};
    auto magic = std::array<char, kPrototypeMagic.size()>{};
    file.read(magic.data(), static_cast<std::streamsize>(magic.size()));
    if (!file) {
      throw std::runtime_error("Prototype file is too small: " + path);
    }

    if (magic != kPrototypeMagic) {
      throw std::runtime_error("Invalid prototype file magic: " + path);
    }

    std::uint32_t version = 0;
    read_binary(file, version, "version");
    if (version != kPrototypeVersion) {
      throw std::runtime_error("Unsupported prototype file version: " + std::to_string(version));
    }

    std::uint32_t gesture_count = 0;
    read_binary(file, store.embedding_dim_, "embedding_dim");
    read_binary(file, gesture_count, "gesture_count");

    if (gesture_count == 0) {
      throw std::runtime_error("Prototype file contains no gestures: " + path);
    }

    store.gestures_.reserve(gesture_count);
    for (std::uint32_t gesture_index = 0; gesture_index < gesture_count; ++gesture_index) {
      auto gesture = GesturePrototypeSet{};
      std::uint32_t sample_count = 0;
      read_binary(file, gesture.gesture_id, "gesture_id");
      read_binary(file, sample_count, "sample_count");
      if (sample_count == 0) {
        throw std::runtime_error("Gesture " + std::to_string(gesture.gesture_id) + " has no prototype samples");
      }

      gesture.samples.reserve(sample_count);
      for (std::uint32_t sample_index = 0; sample_index < sample_count; ++sample_index) {
        auto sample = GesturePrototype{.sample_id = sample_index, .frames = {}};
        std::uint32_t frame_count = 0;
        std::uint32_t embedding_dim = 0;
        read_binary(file, frame_count, "frame_count");
        read_binary(file, embedding_dim, "embedding_dim");
        if (frame_count == 0 || embedding_dim == 0) {
          throw std::runtime_error("Prototype sample has invalid dimensions");
        }
        if (store.embedding_dim_ == 0) {
          store.embedding_dim_ = embedding_dim;
        }
        if (embedding_dim != store.embedding_dim_) {
          throw std::runtime_error("Prototype embedding dimension mismatch: expected " +
                                   std::to_string(store.embedding_dim_) + ", got " +
                                   std::to_string(embedding_dim));
        }

        sample.frames.resize(frame_count);
        for (auto& frame : sample.frames) {
          frame.resize(embedding_dim);
          file.read(reinterpret_cast<char*>(frame.data()),
                    static_cast<std::streamsize>(embedding_dim * sizeof(float)));
          if (!file) {
            throw std::runtime_error("Failed to read prototype frame payload");
          }
        }
        gesture.samples.push_back(std::move(sample));
        ++store.sample_count_;
      }
      store.gestures_.push_back(std::move(gesture));
    }

    return store;
  }

  auto PrototypeStore::gestures() const -> const std::vector<GesturePrototypeSet>& {
    return gestures_;
  }

  auto PrototypeStore::gesture_count() const -> std::size_t {
    return gestures_.size();
  }

  auto PrototypeStore::sample_count() const -> std::size_t {
    return sample_count_;
  }

  auto PrototypeStore::embedding_dim() const -> std::uint32_t {
    return embedding_dim_;
  }

  DtwMatcher::DtwMatcher(float window_ratio) : window_ratio_{window_ratio} {}

  auto DtwMatcher::match(const EncodedSequence& query, const PrototypeStore& store)
    const -> std::vector<Candidate>
  {
    auto candidates = std::vector<Candidate>{};
    candidates.reserve(store.gesture_count());

    for (const auto& gesture : store.gestures()) {
      auto best = Candidate{
        .gesture_id = gesture.gesture_id,
        .sample_id = 0,
        .distance = std::numeric_limits<float>::infinity(),
        .confidence = 0.0F,
      };

      for (const auto& sample : gesture.samples) {
        const auto distance = compute_distance(query, sample.frames);
        if (distance < best.distance) {
          best.sample_id = sample.sample_id;
          best.distance = distance;
        }
      }

      best.confidence = std::isfinite(best.distance) ? (1.0F / (1.0F + best.distance)) : 0.0F;
      candidates.push_back(best);
    }

    std::sort(candidates.begin(), candidates.end(), [](const auto& lhs, const auto& rhs) {
      if (lhs.confidence == rhs.confidence) {
        return lhs.gesture_id < rhs.gesture_id;
      }
      return lhs.confidence > rhs.confidence;
    });

    return candidates;
  }

  auto DtwMatcher::compute_frame_distance(const std::vector<float>& query_frame,
                                          const std::vector<float>& sample_frame) const -> float {
    if (query_frame.size() != sample_frame.size() || query_frame.empty()) {
      return std::numeric_limits<float>::infinity();
    }

    auto sum_sq_diff = 0.0F;
    for (std::size_t i = 0; i < query_frame.size(); ++i) {
      const auto diff = query_frame[i] - sample_frame[i];
      sum_sq_diff += diff * diff;
    }

    return std::sqrt(sum_sq_diff / static_cast<float>(query_frame.size()));
  }

  auto DtwMatcher::compute_window(std::uint32_t query_length,
                                  std::uint32_t sample_length) const -> std::uint32_t {
    const auto max_length = std::max(query_length, sample_length);
    if (window_ratio_ >= 1.0F) {
      return max_length;
    }

    const auto ratio_window = static_cast<std::uint32_t>(
      std::round(static_cast<float>(max_length) * window_ratio_));
    const auto length_diff = query_length > sample_length
      ? query_length - sample_length
      : sample_length - query_length;

    return std::max({length_diff, ratio_window, 1U});
  }

  auto DtwMatcher::compute_distance(const EncodedSequence& query,
                                    const EncodedSequence& sample) const -> float {
    const auto query_length = static_cast<std::uint32_t>(query.size());
    const auto sample_length = static_cast<std::uint32_t>(sample.size());
    if (query_length == 0 || sample_length == 0) {
      return std::numeric_limits<float>::infinity();
    }

    const auto window = compute_window(query_length, sample_length);
    auto prev_cost = std::vector<float>(sample_length + 1, std::numeric_limits<float>::infinity());
    auto prev_steps = std::vector<std::uint32_t>(sample_length + 1, 0);
    prev_cost[0] = 0.0F;

    for (std::uint32_t i = 1; i <= query_length; ++i) {
      auto curr_cost = std::vector<float>(sample_length + 1, std::numeric_limits<float>::infinity());
      auto curr_steps = std::vector<std::uint32_t>(sample_length + 1, 0);

      const auto j_start = (i > window) ? (i - window) : 1U;
      const auto j_end = std::min(sample_length, i + window);

      for (auto j = j_start; j <= j_end; ++j) {
        const auto candidates = std::array<std::pair<float, std::uint32_t>, 3>{{
          {prev_cost[j], prev_steps[j]},
          {curr_cost[j - 1], curr_steps[j - 1]},
          {prev_cost[j - 1], prev_steps[j - 1]},
        }};

        const auto best = *std::min_element(
          candidates.begin(),
          candidates.end(),
          [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });

        const auto frame_cost = compute_frame_distance(query[i - 1], sample[j - 1]);
        curr_cost[j] = best.first + frame_cost;
        curr_steps[j] = best.second + 1;
      }

      prev_cost = std::move(curr_cost);
      prev_steps = std::move(curr_steps);
    }

    if (!std::isfinite(prev_cost[sample_length]) || prev_steps[sample_length] == 0) {
      return std::numeric_limits<float>::infinity();
    }

    return prev_cost[sample_length] / static_cast<float>(prev_steps[sample_length]);
  }

  BilstmEncoder::BilstmEncoder(const std::string& model_path, rknn_core_mask npu_core, float motion_weight)
    : motion_weight_{motion_weight}
  {
    load_model(model_path, npu_core);
    query_io_info();
  }

  BilstmEncoder::~BilstmEncoder() {
    if (ctx_ != 0) {
      rknn_destroy(ctx_);
    }
  }

  auto BilstmEncoder::sequence_length() const -> std::uint32_t {
    return expected_sequence_length_;
  }

  auto BilstmEncoder::embedding_dim() const -> std::uint32_t {
    return frame_embedding_dim_;
  }

  void BilstmEncoder::load_model(const std::string& model_path, rknn_core_mask npu_core) {
    auto model_file = std::ifstream{model_path, std::ios::binary | std::ios::ate};
    if (!model_file.is_open()) {
      throw std::runtime_error("Failed to open RKNN model file: " + model_path);
    }

    const auto model_size = model_file.tellg();
    if (model_size <= 0) {
      throw std::runtime_error("RKNN model file is empty: " + model_path);
    }
    model_file.seekg(0, std::ios::beg);

    auto model_data = std::vector<char>(static_cast<std::size_t>(model_size));
    if (!model_file.read(model_data.data(), model_size)) {
      throw std::runtime_error("Failed to read RKNN model file: " + model_path);
    }

    auto ret = rknn_init(&ctx_, model_data.data(), static_cast<std::uint32_t>(model_data.size()), 0, nullptr);
    if (ret != RKNN_SUCC) {
      throw std::runtime_error("rknn_init failed, ret=" + std::to_string(ret));
    }

    ret = rknn_set_core_mask(ctx_, npu_core);
    if (ret != RKNN_SUCC) {
      spdlog::warn("rknn_set_core_mask failed, ret={}", ret);
    }
  }

  void BilstmEncoder::query_io_info() {
    auto ret = rknn_query(ctx_, RKNN_QUERY_IN_OUT_NUM, &io_num_, sizeof(io_num_));
    if (ret != RKNN_SUCC) {
      throw std::runtime_error("rknn_query IN_OUT_NUM failed, ret=" + std::to_string(ret));
    }
    if (io_num_.n_input != 1 || io_num_.n_output != 1) {
      throw std::runtime_error("Expected 1 input and 1 output, got " +
                               std::to_string(io_num_.n_input) + " inputs and " +
                               std::to_string(io_num_.n_output) + " outputs");
    }

    std::memset(&input_attr_, 0, sizeof(input_attr_));
    input_attr_.index = 0;
    ret = rknn_query(ctx_, RKNN_QUERY_INPUT_ATTR, &input_attr_, sizeof(input_attr_));
    if (ret != RKNN_SUCC) {
      throw std::runtime_error("rknn_query INPUT_ATTR failed, ret=" + std::to_string(ret));
    }
    if (input_attr_.n_dims != 3) {
      throw std::runtime_error("Expected 3D input tensor [batch, seq_len, features], got " +
                               std::to_string(input_attr_.n_dims) + " dimensions");
    }

    expected_sequence_length_ = input_attr_.dims[1];
    const auto expected_feature_dim = input_attr_.dims[2];
    if (expected_feature_dim != kFeatureDim) {
      throw std::runtime_error("Feature dimension mismatch: model expects " +
                               std::to_string(expected_feature_dim) +
                               ", but kFeatureDim=" + std::to_string(kFeatureDim));
    }

    std::memset(&output_attr_, 0, sizeof(output_attr_));
    output_attr_.index = 0;
    ret = rknn_query(ctx_, RKNN_QUERY_OUTPUT_ATTR, &output_attr_, sizeof(output_attr_));
    if (ret != RKNN_SUCC) {
      throw std::runtime_error("rknn_query OUTPUT_ATTR failed, ret=" + std::to_string(ret));
    }
    if (output_attr_.n_dims != 3) {
      throw std::runtime_error("Expected 3D output tensor [batch, seq_len, embedding], got " +
                               std::to_string(output_attr_.n_dims) + " dimensions");
    }

    frame_embedding_dim_ = output_attr_.dims[2];
    input_buffer_.resize(static_cast<std::size_t>(expected_sequence_length_) * kFeatureDim);
  }

  void BilstmEncoder::flatten_features(const std::vector<FeatureVector>& sequence) {
    if (sequence.size() != expected_sequence_length_) {
      throw std::runtime_error("Sequence length mismatch: expected " +
                               std::to_string(expected_sequence_length_) +
                               ", got " + std::to_string(sequence.size()));
    }

    auto offset = std::size_t{0};
    for (const auto& frame : sequence) {
      for (const auto& hand : frame.hands) {
        for (const auto& kp : hand.features) {
          input_buffer_[offset++] = kp.normalized_x;
          input_buffer_[offset++] = kp.normalized_y;
          input_buffer_[offset++] = kp.normalized_z;
          input_buffer_[offset++] = kp.velocity_magnitude * motion_weight_;
        }
      }
    }
  }

  auto BilstmEncoder::encode(const std::vector<FeatureVector>& sequence) -> EncodedSequence {
    flatten_features(sequence);

    auto inputs = std::array<rknn_input, 1>{};
    std::memset(inputs.data(), 0, sizeof(inputs));
    inputs[0].index = 0;
    inputs[0].type = RKNN_TENSOR_FLOAT32;
    inputs[0].size = input_buffer_.size() * sizeof(float);
    inputs[0].fmt = RKNN_TENSOR_NCHW;
    inputs[0].buf = input_buffer_.data();

    auto ret = rknn_inputs_set(ctx_, 1, inputs.data());
    if (ret != RKNN_SUCC) {
      throw std::runtime_error("rknn_inputs_set failed, ret=" + std::to_string(ret));
    }

    ret = rknn_run(ctx_, nullptr);
    if (ret != RKNN_SUCC) {
      throw std::runtime_error("rknn_run failed, ret=" + std::to_string(ret));
    }

    auto outputs = std::array<rknn_output, 1>{};
    std::memset(outputs.data(), 0, sizeof(outputs));
    outputs[0].want_float = 1;

    ret = rknn_outputs_get(ctx_, 1, outputs.data(), nullptr);
    if (ret != RKNN_SUCC) {
      throw std::runtime_error("rknn_outputs_get failed, ret=" + std::to_string(ret));
    }
    const auto guard = RknnOutputReleaseGuard{ctx_, 1, outputs.data()};

    const auto* output_data = static_cast<const float*>(outputs[0].buf);
    const auto output_size = outputs[0].size / sizeof(float);
    const auto expected_output_size = expected_sequence_length_ * frame_embedding_dim_;
    if (output_size != expected_output_size) {
      throw std::runtime_error("Output size mismatch: expected " +
                               std::to_string(expected_output_size) +
                               ", got " + std::to_string(output_size));
    }

    auto encoded_frames = EncodedSequence(expected_sequence_length_);
    for (std::uint32_t frame_index = 0; frame_index < expected_sequence_length_; ++frame_index) {
      encoded_frames[frame_index].assign(
        output_data + frame_index * frame_embedding_dim_,
        output_data + (frame_index + 1) * frame_embedding_dim_);
    }

    return encoded_frames;
  }

  SignlangModel::SignlangModel(const std::string& model_path,
                               const std::string& label_map_path,
                               const std::string& prototypes_path,
                               rknn_core_mask npu_core,
                               float motion_weight,
                               float dtw_window_ratio)
    : encoder_{std::make_unique<BilstmEncoder>(model_path, npu_core, motion_weight)},
      labels_{load_gesture_labels(label_map_path)},
      prototypes_{PrototypeStore::load(prototypes_path)},
      matcher_{dtw_window_ratio}
  {
    if (prototypes_.embedding_dim() != encoder_->embedding_dim()) {
      throw std::runtime_error("Prototype embedding dimension mismatch: encoder outputs " +
                               std::to_string(encoder_->embedding_dim()) +
                               ", prototypes contain " + std::to_string(prototypes_.embedding_dim()));
    }

    spdlog::info("Loaded {} sign labels and {} prototype samples across {} gestures",
                 labels_.labels.size(), prototypes_.sample_count(), prototypes_.gesture_count());
  }

  SignlangModel::~SignlangModel() = default;

  auto SignlangModel::expected_sequence_length() const -> std::uint32_t {
    return encoder_->sequence_length();
  }

  auto SignlangModel::get_gesture_name(std::uint32_t gesture_id) const -> const char* {
    return labels_.name_for(gesture_id);
  }

  auto SignlangModel::infer(const std::vector<FeatureVector>& sequence) -> InferenceResult {
    const auto start_time = std::chrono::steady_clock::now();

    const auto encoded_frames = encoder_->encode(sequence);
    auto candidates = matcher_.match(encoded_frames, prototypes_);

    auto result = InferenceResult{
      .recognized = !candidates.empty(),
      .gesture_id = candidates.empty() ? 0U : candidates.front().gesture_id,
      .inference_time_ms = 0.0F,
      .confidence = candidates.empty() ? 0.0F : candidates.front().confidence,
      .second_confidence = candidates.size() > 1 ? candidates[1].confidence : 0.0F,
      .distance = candidates.empty() ? std::numeric_limits<float>::infinity() : candidates.front().distance,
      .candidates = std::move(candidates),
    };

    const auto end_time = std::chrono::steady_clock::now();
    result.inference_time_ms = std::chrono::duration<float, std::milli>(end_time - start_time).count();
    return result;
  }

} // namespace signlang::signlang_det
