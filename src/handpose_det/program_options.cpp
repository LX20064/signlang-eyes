#include "program_options.hpp"

#include "common/logging_cli.hpp"
#include "cxxopts.hpp"

#include <stdexcept>
#include <string>

namespace signlang::handpose_det {
  namespace {

    void validate_threshold(float value, const char* option_name) {
      if (value <= 0.0F || value >= 1.0F) {
        throw std::runtime_error(std::string("--") + option_name + " must be between 0 and 1");
      }
    }

    auto parse_core_mask(const std::string& value) -> rknn_core_mask {
      if (value == "auto") {
        return RKNN_NPU_CORE_AUTO;
      }
      if (value == "0") {
        return RKNN_NPU_CORE_0;
      }
      if (value == "1") {
        return RKNN_NPU_CORE_1;
      }
      if (value == "2") {
        return RKNN_NPU_CORE_2;
      }
      if (value == "0_1") {
        return RKNN_NPU_CORE_0_1;
      }
      if (value == "0_1_2") {
        return RKNN_NPU_CORE_0_1_2;
      }
      if (value == "all") {
        return RKNN_NPU_CORE_ALL;
      }
      throw std::runtime_error("Unsupported NPU core value: " + value);
    }

  } // namespace

  auto parse_program_options(int argc, char** argv) -> ProgramOptionsParseResult {
    cxxopts::Options options{
        "signlang_eyes_handpose_det",
        "Subscribe video frames from iceoryx2, run MediaPipe palm and hand landmark RKNN models, and publish hand keypoints."};

    options.add_options()("i,input-service", "Upstream video iceoryx2 service name", cxxopts::value<std::string>())(
        "o,output-service", "Output handpose iceoryx2 service name", cxxopts::value<std::string>())(
        "state-event-service", "iceoryx2 event service name for global app state change notifications",
        cxxopts::value<std::string>())("state-blackboard-service",
                                       "iceoryx2 blackboard service name for global app state storage",
                                       cxxopts::value<std::string>())(
        "m,model", "Palm detector RKNN model path",
        cxxopts::value<std::string>()->default_value(kDefaultPalmDetectorModelPath))(
        "landmark-model", "Hand landmark RKNN model path",
        cxxopts::value<std::string>()->default_value(kDefaultLandmarkModelPath))(
        "confidence", "Detection confidence threshold",
        cxxopts::value<float>()->default_value(std::to_string(kDefaultConfidenceThreshold)))(
        "subscriber-buffer", "iceoryx2 subscriber queue size",
        cxxopts::value<std::uint64_t>()->default_value(std::to_string(kDefaultSubscriberBufferSize)))(
        "keypoints", "Expected keypoint count per detection",
        cxxopts::value<std::uint32_t>()->default_value(std::to_string(kDefaultKeypointCount)))(
        "output-hands", "Number of hands published per frame",
        cxxopts::value<std::uint32_t>()->default_value(std::to_string(kDefaultOutputHands)))(
        "npu-core", "RK3588 NPU core mask: auto,0,1,2,0_1,0_1_2,all",
        cxxopts::value<std::string>()->default_value("auto"))(
        "palm-npu-core", "Palm detector NPU core mask; defaults to --npu-core",
        cxxopts::value<std::string>())(
        "landmark-npu-core", "Hand landmark NPU core mask; defaults to --npu-core",
        cxxopts::value<std::string>())("verbose", "Print model tensor details")("h,help",
                                                                                                       "Print usage");
    signlang::logging::add_cli_options(options);

    const auto parsed_options = options.parse(argc, argv);
    if (parsed_options.count("help") != 0) {
      return ProgramUsage{.text = options.help()};
    }

    if (parsed_options.count("input-service") == 0 || parsed_options.count("output-service") == 0) {
      throw std::runtime_error("Options --input-service and --output-service are required.\n\n" + options.help());
    }

    const auto has_state_event_service = parsed_options.count("state-event-service") != 0;
    const auto has_state_blackboard_service = parsed_options.count("state-blackboard-service") != 0;
    if (has_state_event_service != has_state_blackboard_service) {
      throw std::runtime_error("Options --state-event-service and --state-blackboard-service must be provided "
                               "together.\n\n" +
                               options.help());
    }

    const auto confidence_threshold = parsed_options["confidence"].as<float>();
    validate_threshold(confidence_threshold, "confidence");

    const auto subscriber_buffer_size = parsed_options["subscriber-buffer"].as<std::uint64_t>();
    if (subscriber_buffer_size == 0) {
      throw std::runtime_error("--subscriber-buffer must be greater than 0");
    }

    const auto keypoint_count = parsed_options["keypoints"].as<std::uint32_t>();
    if (keypoint_count == 0) {
      throw std::runtime_error("--keypoints must be greater than 0");
    }

    const auto output_hands = parsed_options["output-hands"].as<std::uint32_t>();
    if (output_hands == 0) {
      throw std::runtime_error("--output-hands must be greater than 0");
    }
    if (output_hands > kMaxHandPoseDetections) {
      throw std::runtime_error("--output-hands exceeds handpose payload capacity");
    }

    const auto default_npu_core = parsed_options["npu-core"].as<std::string>();
    const auto palm_npu_core = parsed_options.count("palm-npu-core") != 0
        ? parsed_options["palm-npu-core"].as<std::string>()
        : default_npu_core;
    const auto landmark_npu_core = parsed_options.count("landmark-npu-core") != 0
        ? parsed_options["landmark-npu-core"].as<std::string>()
        : default_npu_core;

    return ProgramOptionsParseResult{ProgramOptions{
        .input_service_name = parsed_options["input-service"].as<std::string>(),
        .output_service_name = parsed_options["output-service"].as<std::string>(),
        .state_event_service_name =
            has_state_event_service ? std::optional<std::string>{parsed_options["state-event-service"].as<std::string>()}
                                    : std::nullopt,
        .state_blackboard_service_name =
            has_state_blackboard_service
                ? std::optional<std::string>{parsed_options["state-blackboard-service"].as<std::string>()}
                : std::nullopt,
        .palm_detector_model_path = parsed_options["model"].as<std::string>(),
        .landmark_model_path = parsed_options["landmark-model"].as<std::string>(),
        .confidence_threshold = confidence_threshold,
        .subscriber_buffer_size = subscriber_buffer_size,
        .keypoint_count = keypoint_count,
        .output_hands = output_hands,
        .palm_detector_npu_core_mask = parse_core_mask(palm_npu_core),
        .landmark_npu_core_mask = parse_core_mask(landmark_npu_core),
        .verbose = parsed_options.count("verbose") != 0,
        .logging = signlang::logging::parse_cli_options(parsed_options),
    }};
  }

} // namespace signlang::handpose_det
