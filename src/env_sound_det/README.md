# env_sound_det — Environmental Sound Detection (YAMNet)

## Overview

The **env_sound_det** module performs real-time environmental sound classification using Google's YAMNet (MobileNetV1-based) model running on the RKNN NPU. It subscribes to audio frames, processes them through a sliding window, runs inference, and requests alert-state changes for configured horn/dangerous-sound labels.

- **Executable**: `env_sound_det` (installed under `bin/`)
- **IPC Pattern**: Publish-Subscribe (audio subscriber) + Request-Response (state control)
- **Input**: `signlang::audio_frontend::AudioFrame` from iceoryx2
- **Output**: `DangerousSound` state-control requests when configured horn labels are detected
- **Model**: YAMNet (521-class environmental sound classifier, RKNN-accelerated)

## File Inventory

| File | Description |
|------|-------------|
| `main.cpp` | Entry point; signal handling, main loop |
| `program_options.{cpp,hpp}` | CLI argument parsing via cxxopts |
| `yamnet_model.{cpp,hpp}` | YAMNet RKNN model: load, inference, score averaging, Top-K selection |
| `iceoryx_gateway.{cpp,hpp}` | iceoryx2 subscriber (audio) and request-response client (state control) |
| `audio_ring_buffer.{cpp,hpp}` | Thread-safe circular buffer for audio window accumulation |

## Command-Line Parameters

### IPC (Required)

| Parameter | Description |
|-----------|-------------|
| `--input-service` / `-i` | iceoryx2 audio input publish-subscribe service name |
| `--state-control-service` | iceoryx2 app state control request-response service name |

### Model Paths

| Parameter | Default | Description |
|-----------|---------|-------------|
| `--model` / `-m` | `models/yamnet/yamnet_3s.rknn` | YAMNet RKNN model path |
| `--class-map` | `models/yamnet/yamnet_class_map.txt` | YAMNet 521-class label mapping file |

### Processing

| Parameter | Default | Range | Description |
|-----------|---------|-------|-------------|
| `--window-ms` / `-w` | `3000` | `100–60000` | Detection window duration in milliseconds |
| `--overlap` | `0.2` | `[0.0, 1.0)` | Overlap ratio between adjacent windows |
| `--top-k` | `5` | `1–5` | Number of highest-scoring classes to evaluate for dangerous sound labels |

### Performance

| Parameter | Default | Description |
|-----------|---------|-------------|
| `--npu-core` | `auto` | NPU core mask: `auto`, `all`, `0`, `1`, `2`, `0_1`, `0_1_2` |
| `--rknn-priority` | `medium` | RKNN context priority: `high`, `medium`, `low` |
| `--poll-ms` | `2` | `1–100` | Subscriber polling sleep in ms when no sample is ready |
| `--subscriber-buffer` | `2` | `≥1` | iceoryx2 subscriber queue size |

## Technical Details

### YAMNet Architecture

- **Backbone**: MobileNetV1 with depthwise-separable convolutions
- **Input**: `[1, 48000]` — 3 seconds of audio @ 16 kHz mono
- **Output**: `[N_frames × 521]` — per-frame class scores (N_frames varies with model)
- **Post-processing**: Average scores across all output frames, sort, select Top-K

### Inference Pipeline

1. Accumulate samples in ring buffer (sliding window with configurable overlap)
2. If audio window matches model input length, pass directly; otherwise zero-pad tail
3. Run single-pass RKNN inference
4. Average per-class scores across all time frames
5. Sort descending, take Top-K, copy labels from class map

### State Control (Dangerous Sound Detection)

When the module detects one of the configured horn labels (`Air horn, truck horn`, `Vehicle horn, car horn, honking`, or `Train horn`), it requests a `DangerousSound` state change via the iceoryx2 Request-Response control service.

### Class Examples

| ID | Label |
|----|-------|
| 0 | Speech |
| 6 | Shout |
| 292 | Fire |
| 302 | Vehicle horn, car horn, honking |
| 304 | Car alarm |
| 312 | Air horn, truck horn |
| 316 | Emergency vehicle |
| 325 | Train horn |
| 421 | Gunshot, gunfire |

See `models/yamnet/yamnet_class_map.txt` for all 521 classes.

## Usage Example

```bash
./env_sound_det \
    --input-service audio_capture \
    --state-control-service app_state_control \
    --window-ms 3000 \
    --overlap 0.2 \
    --top-k 5
```
