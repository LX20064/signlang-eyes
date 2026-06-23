#ifndef SIGNLANG_EYES_HANDPOSE_DET_PROGRAM_OPTIONS_HPP
#define SIGNLANG_EYES_HANDPOSE_DET_PROGRAM_OPTIONS_HPP

#include "common/logging.hpp"
#include "handpose_frame.hpp"
#include "rknn_api.h"

#include <cstdint>
#include <optional>
#include <string>
#include <variant>

namespace signlang::handpose_det {

  constexpr auto kDefaultPalmDetectorModelPath = "models/mediapipe/hand_detector.rknn";
  constexpr auto kDefaultLandmarkModelPath = "models/mediapipe/hand_landmarks_detector.rknn";
  constexpr auto kDefaultConfidenceThreshold = 0.5F;
  constexpr auto kDefaultSubscriberBufferSize = std::uint64_t{2};
  constexpr auto kDefaultKeypointCount = std::uint32_t{21};
  constexpr auto kDefaultOutputHands = std::uint32_t{2};

  struct ProgramOptions {
    std::string input_service_name;
    std::string output_service_name;
    std::optional<std::string> state_event_service_name;
    std::optional<std::string> state_blackboard_service_name;
    std::string palm_detector_model_path;
    std::string landmark_model_path;
    float confidence_threshold;
    std::uint64_t subscriber_buffer_size;
    std::uint32_t keypoint_count;
    std::uint32_t output_hands;
    rknn_core_mask palm_detector_npu_core_mask;
    rknn_core_mask landmark_npu_core_mask;
    bool verbose;
    signlang::logging::Options logging;
  };

  struct ProgramUsage {
    std::string text;
  };

  using ProgramOptionsParseResult = std::variant<ProgramOptions, ProgramUsage>;

  auto parse_program_options(int argc, char** argv) -> ProgramOptionsParseResult;

} // namespace signlang::handpose_det

#endif // SIGNLANG_EYES_HANDPOSE_DET_PROGRAM_OPTIONS_HPP
