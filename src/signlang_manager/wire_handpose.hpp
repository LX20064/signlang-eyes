#ifndef SIGNLANG_EYES_SIGNLANG_MANAGER_WIRE_HANDPOSE_HPP
#define SIGNLANG_EYES_SIGNLANG_MANAGER_WIRE_HANDPOSE_HPP

#include "handpose_det/handpose_frame.hpp"

#include <cstdint>
#include <vector>

namespace signlang::signlang_manager {

  constexpr auto kWireHandposeFormatRawF32 = std::uint8_t{1};

  struct WireHandposeFrame {
    handpose_det::HandPoseFrameMetadata metadata{};
    std::vector<handpose_det::HandPoseDetection> detections;
  };

  auto encode_wire_handpose_frame(const handpose_det::HandPoseFrameMetadata& metadata,
                                  const handpose_det::HandPoseDetection* detections, std::uint32_t detection_count,
                                  std::uint32_t max_detections) -> std::vector<std::uint8_t>;
  auto decode_wire_handpose_frame(const std::vector<std::uint8_t>& payload) -> WireHandposeFrame;

} // namespace signlang::signlang_manager

#endif // SIGNLANG_EYES_SIGNLANG_MANAGER_WIRE_HANDPOSE_HPP
