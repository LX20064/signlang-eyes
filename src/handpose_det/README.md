# handpose_det - MediaPipe Hand Pose Detection

## Overview

`handpose_det` subscribes to RGB24 video frames, runs the MediaPipe palm detector and hand landmark RKNN models, and publishes fixed-size hand pose payloads through iceoryx2.

- Palm detector: `models/mediapipe/hand_detector.rknn`
- Landmark detector: `models/mediapipe/hand_landmarks_detector.rknn`
- Scaling/cropping: Rockchip RGA (`librga`)
- Output keypoints: 21 landmarks with `x`, `y`, `z`, and `confidence`

## Output Semantics

The module always publishes exactly `--output-hands` hand slots per frame.

1. Run palm detection on the full frame.
2. Filter palms by `--confidence`.
3. If more palms pass than requested, keep the highest-confidence palms.
4. Sort the selected palms left-to-right by detection box center.
5. Run the landmark model only for those selected palms.
6. If fewer palms pass than requested, leave the remaining output slots zero-filled.

`HandPoseFrameMetadata::detection_count` and `payload_count` are set to `--output-hands`, so zero-filled slots are part of the published payload by design.

## Command-Line Parameters

| Parameter | Default | Description |
|---|---:|---|
| `--input-service`, `-i` | required | Upstream video iceoryx2 service |
| `--output-service`, `-o` | required | Hand pose output iceoryx2 service |
| `--model`, `-m` | `models/mediapipe/hand_detector.rknn` | Palm detector RKNN model |
| `--landmark-model` | `models/mediapipe/hand_landmarks_detector.rknn` | Hand landmark RKNN model |
| `--output-hands` | `2` | Fixed number of hand slots to publish |
| `--confidence` | `0.5` | Palm detection confidence threshold |
| `--npu-core` | `auto` | RK3588 NPU core mask |
| `--palm-npu-core` | `--npu-core` | Palm detector NPU core override |
| `--landmark-npu-core` | `--npu-core` | Hand landmark model NPU core override |
| `--subscriber-buffer` | `2` | iceoryx2 subscriber queue size |
| `--verbose` | off | Print RKNN tensor details |

## IPC Payload

```cpp
struct HandPoseKeypoint {
  float x;
  float y;
  float z;
  float confidence;
};

struct HandPoseDetection {
  HandPoseBox box;
  std::array<HandPoseKeypoint, 21> keypoints;
  float confidence;
  std::uint32_t class_id;
};
```

Coordinates are in source image pixel space. Landmark `z` is scaled from the 224x224 landmark crop back to the crop scale.

## Usage

```bash
./handpose_det \
  --input-service video_capture \
  --output-service handpose_result \
  --output-hands 2 \
  --confidence 0.5
```

With state gating:

```bash
./handpose_det \
  --input-service video_capture \
  --output-service handpose_result \
  --state-event-service app_state_event \
  --state-blackboard-service app_state_blackboard
```
