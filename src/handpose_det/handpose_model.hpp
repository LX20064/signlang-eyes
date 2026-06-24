#ifndef SIGNLANG_EYES_HANDPOSE_DET_HANDPOSE_MODEL_HPP
#define SIGNLANG_EYES_HANDPOSE_DET_HANDPOSE_MODEL_HPP

#include "handpose_frame.hpp"
#include "program_options.hpp"
#include "rknn_api.h"
#include "video_frontend/video_frame.hpp"

#include "iox2/bb/slice.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace signlang::handpose_det {

  struct InferenceResult {
    std::uint32_t detection_count;
    std::uint32_t image_width;
    std::uint32_t image_height;
    std::uint32_t model_width;
    std::uint32_t model_height;
  };

  class HandPoseModel {
  public:
    HandPoseModel(std::string palm_detector_model_path, std::string landmark_model_path, const ProgramOptions& options);
    ~HandPoseModel();

    HandPoseModel(const HandPoseModel&) = delete;
    auto operator=(const HandPoseModel&) -> HandPoseModel& = delete;
    HandPoseModel(HandPoseModel&&) = delete;
    auto operator=(HandPoseModel&&) -> HandPoseModel& = delete;

    auto run(const signlang::video_frontend::VideoFrameMetadata& metadata, const std::uint8_t* payload,
             std::uint64_t payload_size, iox2::bb::MutableSlice<HandPoseDetection> detections) -> InferenceResult;

  private:
    struct RknnModel;
    struct Anchor;
    struct PalmCandidate {
      HandPoseDetection detection;
      std::array<float, 14> palm_keypoints;
    };
    struct CropTransform;

    void initialize_models(const ProgramOptions& options);
    void validate_models() const;
    void print_tensor_details() const;
    void build_palm_anchors();
    void run_palm_detector(const signlang::video_frontend::VideoFrameMetadata& metadata, const std::uint8_t* payload,
                           std::uint64_t payload_size);
    void run_landmark_detector(const signlang::video_frontend::VideoFrameMetadata& metadata,
                               const std::uint8_t* payload, std::uint64_t payload_size,
                               PalmCandidate& candidate);
    auto crop_transform_for(const HandPoseBox& box, std::uint32_t image_width, std::uint32_t image_height) const
        -> CropTransform;

    std::string palm_detector_model_path_;
    std::string landmark_model_path_;
    std::unique_ptr<RknnModel> palm_detector_;
    std::unique_ptr<RknnModel> landmark_model_;
    std::vector<Anchor> anchors_;
    std::vector<PalmCandidate> candidates_;
    std::vector<PalmCandidate> selected_;
    float confidence_threshold_;
    std::uint32_t keypoint_count_;
    std::uint32_t output_hands_;
    std::uint32_t model_width_;
    std::uint32_t model_height_;
  };

} // namespace signlang::handpose_det

#endif // SIGNLANG_EYES_HANDPOSE_DET_HANDPOSE_MODEL_HPP
