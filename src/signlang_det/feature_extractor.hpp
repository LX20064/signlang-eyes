#ifndef SIGNLANG_EYES_EDGEAI_SIGNLANG_DET_FEATURE_EXTRACTOR_HPP
#define SIGNLANG_EYES_EDGEAI_SIGNLANG_DET_FEATURE_EXTRACTOR_HPP

#include "signlang_result.hpp"
#include "handpose_det/handpose_frame.hpp"

#include <array>
#include <cstdint>
#include <optional>

namespace signlang::signlang_det {

class FeatureExtractor {
public:
  explicit FeatureExtractor(float min_confidence);

  auto extract(const handpose_det::HandPoseFrameMetadata& metadata,
               const handpose_det::HandPoseDetection* detections,
               std::uint32_t detection_count)
    -> std::optional<FeatureVector>;

  void reset();

private:
  auto select_best_hand(const handpose_det::HandPoseDetection* detections,
                        std::uint32_t count) const
    -> const handpose_det::HandPoseDetection*;

  auto compute_bounding_box_scale(
    const std::array<handpose_det::HandPoseKeypoint, handpose_det::kHandPoseKeypointCount>& keypoints) const
    -> float;

  auto compute_velocity_magnitudes(
    const std::array<handpose_det::HandPoseKeypoint, handpose_det::kHandPoseKeypointCount>& current,
    float scale) const
    -> std::array<float, handpose_det::kHandPoseKeypointCount>;

  float min_confidence_;
  std::optional<std::array<handpose_det::HandPoseKeypoint, handpose_det::kHandPoseKeypointCount>> prev_keypoints_;
  std::uint64_t prev_sequence_number_{0};
};

} // namespace signlang::signlang_det

#endif // SIGNLANG_EYES_EDGEAI_SIGNLANG_DET_FEATURE_EXTRACTOR_HPP
