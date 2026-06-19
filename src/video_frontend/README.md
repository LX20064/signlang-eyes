# video_frontend — Video Capture & Publishing

## Overview

The **video_frontend** module captures video frames from a V4L2 (Video4Linux2) camera device and publishes them as `VideoFrame` messages over an iceoryx2 publish-subscribe service. It supports YUYV format capture with optional resize (nearest-neighbor downscaling) and configurable frame rate.

- **Executable**: `signlang_eyes_edgeai_video_frontend`
- **IPC Pattern**: Publish-Subscribe (producer)
- **Input**: V4L2 camera device (YUYV 4:2:2 format)
- **Output**: `signlang::video_frontend::VideoFrame` on iceoryx2

## File Inventory

| File | Description |
|------|-------------|
| `main.cpp` | Entry point; signal handling (SIGINT/SIGTERM), main capture loop |
| `program_options.{cpp,hpp}` | CLI argument parsing via cxxopts |
| `v4l2_capture_device.{cpp,hpp}` | V4L2 device enumeration, format negotiation, frame capture |
| `video_format.hpp` | `VideoFormat`, `VideoFormatRequest` structs and dimension constants |
| `video_frame.hpp` | `VideoFrame` and `VideoFrameMetadata` IPC message definitions (shared header) |
| `video_processor.{cpp,hpp}` | YUYV resize (nearest-neighbor), format conversion |
| `video_publisher.{cpp,hpp}` | iceoryx2 publish-subscribe publisher wrapper with payload management |

## Command-Line Parameters

| Parameter | Default | Range | Description |
|-----------|---------|-------|-------------|
| `--device` / `-d` | *(required)* | — | V4L2 camera device name (e.g., `/dev/video0`) |
| `--service` / `-s` | *(required)* | — | iceoryx2 publish-subscribe service name for video output |
| `--capture-width` | (device default) | `1–3840` | Requested camera capture width in pixels |
| `--capture-height` | (device default) | `1–3840` | Requested camera capture height in pixels |
| `--output-width` | (matches capture) | `1–3840` | Published output width in pixels (≤ capture width) |
| `--output-height` | (matches capture) | `1–3840` | Published output height in pixels (≤ capture height) |
| `--fps` | `30` | `1–240` | Requested camera frame rate |
| `--help` / `-h` | — | — | Print usage |

> **Note**: `--capture-width` and `--capture-height` must be specified together (or omitted together). Same rule applies to `--output-width` and `--output-height`.

## Technical Details

### Video Format

- **Pixel Format**: YUYV 4:2:2 (2 bytes per pixel pair)
- **Output Frame Size**: `width × height × 2` bytes
- **Resize**: Nearest-neighbor interpolation when capture and output dimensions differ

### YUYV Resize Logic

When output resolution differs from capture resolution:
1. Source row indices are precomputed via integer scaling
2. YUYV pair mappings track luma (Y) and chroma (U/V) sample offsets
3. Nearest-neighbor selection for each output pixel pair

### VideoFrame Structure

Each published `VideoFrame` contains:
- **Metadata**: Width, height, pixel format, frame size in bytes
- **Timestamp**: Captured via `clock_gettime(CLOCK_MONOTONIC)`
- **Sequence number**: Monotonically increasing frame counter
- **Payload**: Raw YUYV pixel data as a mutable byte slice

### Capture Loop

1. Dequeue buffer from V4L2
2. Process (resize if needed)
3. Publish via iceoryx2
4. Requeue buffer to V4L2 driver

## Dependencies

- **V4L2** (Linux kernel API): Camera capture
- **iceoryx2**: Zero-copy IPC publishing
- **pthread**: Thread synchronization

## Usage Example

```bash
# Basic usage — capture from /dev/video0 at default resolution and 30 fps
./signlang_eyes_edgeai_video_frontend \
    --device /dev/video0 \
    --service video_capture

# Custom resolution and frame rate
./signlang_eyes_edgeai_video_frontend \
    --device /dev/video0 \
    --service video_capture \
    --capture-width 1920 \
    --capture-height 1080 \
    --output-width 640 \
    --output-height 480 \
    --fps 60

# List available V4L2 devices and formats
v4l2-ctl --list-devices
v4l2-ctl -d /dev/video0 --list-formats-ext
```
