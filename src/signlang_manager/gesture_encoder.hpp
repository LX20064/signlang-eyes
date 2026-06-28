#ifndef SIGNLANG_EYES_SIGNLANG_MANAGER_GESTURE_ENCODER_HPP
#define SIGNLANG_EYES_SIGNLANG_MANAGER_GESTURE_ENCODER_HPP

#include "gesture_types.hpp"

#include "rknn_api.h"

#include <string>
#include <vector>

namespace signlang::signlang_manager {

  class GestureEncoder {
  public:
    GestureEncoder(const std::string& model_path, rknn_core_mask npu_core, float motion_weight);

    GestureEncoder(const GestureEncoder&) = delete;
    auto operator=(const GestureEncoder&) -> GestureEncoder& = delete;
    GestureEncoder(GestureEncoder&&) = delete;
    auto operator=(GestureEncoder&&) -> GestureEncoder& = delete;

    ~GestureEncoder();

    [[nodiscard]] auto encode(const std::vector<FeatureVector>& sequence) -> EncodedSequence;
    [[nodiscard]] auto sequence_length() const -> std::uint32_t;
    [[nodiscard]] auto embedding_dim() const -> std::uint32_t;

  private:
    void load_model(const std::string& model_path, rknn_core_mask npu_core);
    void query_io_info();
    void flatten_features(const std::vector<FeatureVector>& sequence);

    rknn_context ctx_{0};
    rknn_input_output_num io_num_{};
    rknn_tensor_attr input_attr_{};
    rknn_tensor_attr output_attr_{};
    std::uint32_t expected_sequence_length_{0};
    std::uint32_t frame_embedding_dim_{0};
    std::vector<float> input_buffer_;
    float motion_weight_;
  };

} // namespace signlang::signlang_manager

#endif // SIGNLANG_EYES_SIGNLANG_MANAGER_GESTURE_ENCODER_HPP
