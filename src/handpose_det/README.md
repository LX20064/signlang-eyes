# handpose_det — MediaPipe Hand Pose Detection

## Overview

The **handpose_det** module subscribes to RGB24 video frames, runs the MediaPipe dual-model pipeline (palm detector + hand landmark detector) using RKNN NPU inference, and publishes fixed-size hand pose payloads through iceoryx2. It supports state-based enable/disable control.

- **Executable**: `handpose_det` (installed under `bin/`)
- **IPC Pattern**: Publish-Subscribe (video subscriber + hand pose publisher) + Event/Blackboard (state control)
- **Input**: RGB24 video frames with `VideoFrameMetadata` user header from iceoryx2
- **Output**: `iox2::bb::Slice<HandPoseDetection>` with `HandPoseFrameMetadata` user header on iceoryx2
- **Models**: Palm detector (192×192) + Hand landmarks detector (224×224), both RKNN-accelerated
- **Preprocessing**: Rockchip RGA for crop/scale operations

## Output Semantics

The module always publishes exactly `--output-hands` hand slots per frame (default 2).

**Detection pipeline:**
1. Run palm detection on the full frame (192×192 input, RKNN NPU)
2. Filter palms by `--confidence` threshold
3. If more palms pass than requested slots, keep the highest-confidence palms
4. Sort the selected palms **left-to-right** by detection box center x-coordinate
5. Run the landmark model (224×224 input, RKNN NPU) only for selected palms
6. If fewer palms pass than requested slots, zero-fill the remaining output slots

**Key properties:**
- `HandPoseFrameMetadata::detection_count` and `payload_count` are always set to `--output-hands`
- Zero-filled slots have `present = false` in the detection structure
- Consistent slot ordering (left-to-right) enables temporal hand tracking in downstream modules
- Output keypoints: 21 landmarks per hand with `x`, `y`, `z` (relative depth), and `confidence`

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
  float x;              // Pixel x-coordinate in source image space
  float y;              // Pixel y-coordinate in source image space
  float z;              // Relative depth (scaled from 224×224 crop to source scale)
  float confidence;     // Keypoint confidence (0.0-1.0)
};

struct HandPoseBox {
  float x_center;       // Detection box center x
  float y_center;       // Detection box center y
  float width;          // Detection box width
  float height;         // Detection box height
  float rotation;       // Box rotation in radians
};

struct HandPoseDetection {
  HandPoseBox box;
  std::array<HandPoseKeypoint, 21> keypoints;  // MediaPipe 21-landmark topology
  float confidence;     // Palm detection confidence
  std::uint32_t class_id;
  bool present;         // false for zero-filled slots
};
```

Coordinates are in source image pixel space. Landmark `z` is scaled from the 224×224 landmark crop back to the source image scale.

## Usage

```bash
./handpose_det \
  --input-service video_capture \
  --output-service handpose_result \
  --output-hands 2 \
  --confidence 0.5
```

With state gating (only process frames when in sign language mode):

```bash
./handpose_det \
  --input-service video_capture \
  --output-service handpose_result \
  --state-event-service app_state_event \
  --state-blackboard-service app_state_blackboard
```

## State Control

When both state gate services are provided, the module reads the current blackboard state at startup and uses the iceoryx2 Event + Blackboard pattern:
- **Enabled states**: `SignLanguageChat`, `SignLanguageAi`
- **Disabled states**: `Normal`, `Asr`, `DangerousSound`
- **When disabled**: Event-driven idle, zero CPU usage while waiting for state changes
- **When enabled**: Process every video frame
- **Without state gate services**: Always enabled (no state control)

## Performance Characteristics

- **Palm detection**: ~8-15ms on single NPU core (RK3588)
- **Landmark detection**: ~5-10ms per hand on single NPU core
- **Total latency**: ~20-35ms per frame for 2 hands (30 fps capable)
- **RGA preprocessing**: ~2-3ms for crop/scale operations per hand
- **Memory**: ~15MB model footprint (palm + landmark models)
- **CPU usage**: <8% on single core (Cortex-A76 @ 2.4GHz)
