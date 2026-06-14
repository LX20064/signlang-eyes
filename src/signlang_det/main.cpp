#include "feature_extractor.hpp"
#include "iceoryx_gateway.hpp"
#include "keypoint_ring_buffer.hpp"
#include "program_options.hpp"
#include "signlang_model.hpp"
#include "signlang_result.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <exception>
#include <iostream>
#include <mutex>
#include <thread>
#include <variant>

namespace {

volatile std::sig_atomic_t g_should_stop = 0;

void handle_shutdown_signal(int) {
  g_should_stop = 1;
}

void install_signal_handlers() {
  std::signal(SIGINT, handle_shutdown_signal);
  std::signal(SIGTERM, handle_shutdown_signal);
}

auto build_result(
  const std::vector<signlang::signlang_det::FeatureVector>& window,
  const signlang::signlang_det::SignlangModel::InferenceResult& inference,
  const signlang::signlang_det::ProgramOptions& options,
  const signlang::signlang_det::SignlangModel& model)
  -> signlang::signlang_det::SignlangResult
{
  using signlang::signlang_det::SignlangResult;
  using signlang::signlang_det::copy_string;
  using signlang::signlang_det::steady_timestamp_ns;

  SignlangResult result;
  result.timestamp_ns = steady_timestamp_ns();
  result.window_start_sequence = window.front().source_sequence_number;
  result.window_end_sequence = window.back().source_sequence_number;
  result.sequence_length = options.sequence_length;
  result.overlap_ratio = options.overlap_ratio;
  result.inference_time_ms = inference.inference_time_ms;
  result.gesture_id = inference.gesture_id;

  const char* gesture_name = model.get_gesture_name(inference.gesture_id);
  copy_string(gesture_name, result.gesture_name);

  return result;
}

void receiver_loop(
  const signlang::signlang_det::ProgramOptions& options,
  signlang::signlang_det::KeypointRingBuffer& ring_buffer,
  std::mutex& buffer_mutex,
  std::condition_variable& buffer_cv,
  const std::atomic<bool>& should_stop)
{
  using signlang::signlang_det::IpcHandposeSubscriber;
  using signlang::signlang_det::FeatureExtractor;

  IpcHandposeSubscriber subscriber(options.input_service_name,
                                     options.subscriber_buffer_size);
  FeatureExtractor extractor(options.min_keypoint_confidence);

  while (!should_stop) {
    const bool received = subscriber.receive_latest(
      [&](const auto& metadata, const auto* detections, auto count) {
        if (auto feature = extractor.extract(metadata, detections, count)) {
          {
            std::lock_guard lock(buffer_mutex);
            ring_buffer.push(*feature);
          }
          buffer_cv.notify_one();
        }
      });

    if (!received) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }
}

void inference_loop(
  const signlang::signlang_det::ProgramOptions& options,
  signlang::signlang_det::KeypointRingBuffer& ring_buffer,
  std::mutex& buffer_mutex,
  std::condition_variable& buffer_cv,
  const std::atomic<bool>& should_stop)
{
  using signlang::signlang_det::SignlangModel;
  using signlang::signlang_det::IpcSignlangPublisher;

  SignlangModel model(options.model_path, options.label_map_path,
                      options.npu_core_mask);
  IpcSignlangPublisher publisher(options.output_service_name);

  const auto hop_frames = static_cast<std::uint32_t>(
    options.sequence_length * (1.0f - options.overlap_ratio));
  std::uint64_t last_processed_seq = 0;

  while (!should_stop) {
    std::unique_lock lock(buffer_mutex);
    buffer_cv.wait(lock, [&] {
      return should_stop.load() || ring_buffer.size() >= options.sequence_length;
    });

    if (should_stop) break;

    auto window = ring_buffer.get_window(options.sequence_length);
    if (!window.has_value()) continue;

    const auto window_end_seq = window->back().source_sequence_number;
    if (window_end_seq < last_processed_seq + hop_frames) {
      continue;
    }

    lock.unlock();

    try {
      auto inference_result = model.infer(*window);
      auto result = build_result(*window, inference_result, options, model);
      publisher.publish(result);

      last_processed_seq = window_end_seq;
    } catch (const std::exception& e) {
      std::cerr << "Inference error: " << e.what() << std::endl;
    }
  }
}

} // namespace

auto main(int argc, char** argv) -> int {
  using signlang::signlang_det::parse_program_options;
  using signlang::signlang_det::ProgramOptions;
  using signlang::signlang_det::ProgramUsage;
  using signlang::signlang_det::KeypointRingBuffer;
  using signlang::signlang_det::compute_buffer_capacity;

  try {
    const auto parse_result = parse_program_options(argc, argv);
    if (const auto* usage = std::get_if<ProgramUsage>(&parse_result); usage != nullptr) {
      std::cout << usage->text << '\n';
      return 0;
    }

    const auto options = std::get<ProgramOptions>(parse_result);
    install_signal_handlers();

    const auto buffer_capacity = compute_buffer_capacity(
      options.sequence_length, options.overlap_ratio);
    KeypointRingBuffer ring_buffer(buffer_capacity);
    std::mutex buffer_mutex;
    std::condition_variable buffer_cv;
    std::atomic<bool> should_stop{false};

    std::thread receiver_thread([&]() {
      receiver_loop(options, ring_buffer, buffer_mutex, buffer_cv, should_stop);
    });

    std::thread inference_thread([&]() {
      inference_loop(options, ring_buffer, buffer_mutex, buffer_cv, should_stop);
    });

    while (g_should_stop == 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    should_stop = true;
    buffer_cv.notify_all();

    receiver_thread.join();
    inference_thread.join();

    return 0;
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
}
