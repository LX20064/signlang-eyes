#include "wire_handpose.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace signlang::signlang_manager {
  namespace {

    void append_u8(std::vector<std::uint8_t>& out, std::uint8_t value) { out.push_back(value); }

    void append_u16(std::vector<std::uint8_t>& out, std::uint16_t value) {
      out.push_back(static_cast<std::uint8_t>(value & 0xFFU));
      out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    }

    void append_u32(std::vector<std::uint8_t>& out, std::uint32_t value) {
      out.push_back(static_cast<std::uint8_t>(value & 0xFFU));
      out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
      out.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
      out.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
    }

    void append_u64(std::vector<std::uint8_t>& out, std::uint64_t value) {
      for (auto shift = 0U; shift < 64U; shift += 8U) {
        out.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFFU));
      }
    }

    void append_f32(std::vector<std::uint8_t>& out, float value) {
      auto bits = std::uint32_t{0};
      std::memcpy(&bits, &value, sizeof(bits));
      append_u32(out, bits);
    }

    auto read_u8(const std::vector<std::uint8_t>& payload, std::size_t& offset) -> std::uint8_t {
      if (offset >= payload.size()) {
        throw std::runtime_error("Wire handpose payload is truncated");
      }
      return payload[offset++];
    }

    auto read_u16(const std::vector<std::uint8_t>& payload, std::size_t& offset) -> std::uint16_t {
      if (offset + 2U > payload.size()) {
        throw std::runtime_error("Wire handpose payload is truncated");
      }
      const auto value = static_cast<std::uint16_t>(payload[offset]) |
          static_cast<std::uint16_t>(static_cast<std::uint16_t>(payload[offset + 1U]) << 8U);
      offset += 2U;
      return value;
    }

    auto read_u32(const std::vector<std::uint8_t>& payload, std::size_t& offset) -> std::uint32_t {
      if (offset + 4U > payload.size()) {
        throw std::runtime_error("Wire handpose payload is truncated");
      }
      const auto value = static_cast<std::uint32_t>(payload[offset]) |
          (static_cast<std::uint32_t>(payload[offset + 1U]) << 8U) |
          (static_cast<std::uint32_t>(payload[offset + 2U]) << 16U) |
          (static_cast<std::uint32_t>(payload[offset + 3U]) << 24U);
      offset += 4U;
      return value;
    }

    auto read_u64(const std::vector<std::uint8_t>& payload, std::size_t& offset) -> std::uint64_t {
      auto value = std::uint64_t{0};
      for (auto shift = 0U; shift < 64U; shift += 8U) {
        value |= static_cast<std::uint64_t>(read_u8(payload, offset)) << shift;
      }
      return value;
    }

    auto read_f32(const std::vector<std::uint8_t>& payload, std::size_t& offset) -> float {
      const auto bits = read_u32(payload, offset);
      auto value = float{0.0F};
      std::memcpy(&value, &bits, sizeof(value));
      return value;
    }

  } // namespace

  auto encode_wire_handpose_frame(const handpose_det::HandPoseFrameMetadata& metadata,
                                  const handpose_det::HandPoseDetection* detections, std::uint32_t detection_count,
                                  std::uint32_t max_detections) -> std::vector<std::uint8_t> {
    const auto output_count = std::min(detection_count, max_detections);
    auto out = std::vector<std::uint8_t>{};
    out.reserve(48U + static_cast<std::size_t>(output_count) * 368U);

    append_u8(out, kWireHandposeFormatRawF32);
    append_u8(out, static_cast<std::uint8_t>(output_count));
    append_u16(out, static_cast<std::uint16_t>(handpose_det::kHandPoseKeypointCount));
    append_u64(out, metadata.sequence_number);
    append_u64(out, metadata.timestamp_ns);
    append_u64(out, metadata.source_sequence_number);
    append_u64(out, metadata.source_timestamp_ns);
    append_u32(out, metadata.image_width);
    append_u32(out, metadata.image_height);
    append_u32(out, metadata.model_width);
    append_u32(out, metadata.model_height);

    for (std::uint32_t i = 0; i < output_count; ++i) {
      const auto& detection = detections[i];
      append_u8(out, detection.present ? 1U : 0U);
      append_u8(out, detection.is_left_hand ? 1U : 0U);
      append_u16(out, static_cast<std::uint16_t>(detection.class_id));
      append_f32(out, detection.confidence);
      append_f32(out, detection.presence_confidence);
      append_f32(out, detection.box.left);
      append_f32(out, detection.box.top);
      append_f32(out, detection.box.right);
      append_f32(out, detection.box.bottom);
      for (const auto& keypoint : detection.keypoints) {
        append_f32(out, keypoint.x);
        append_f32(out, keypoint.y);
        append_f32(out, keypoint.z);
        append_f32(out, keypoint.confidence);
      }
    }

    return out;
  }

  auto decode_wire_handpose_frame(const std::vector<std::uint8_t>& payload) -> WireHandposeFrame {
    auto offset = std::size_t{0};
    const auto format = read_u8(payload, offset);
    if (format != kWireHandposeFormatRawF32) {
      throw std::runtime_error("Unsupported wire handpose format");
    }

    const auto detection_count = read_u8(payload, offset);
    const auto keypoint_count = read_u16(payload, offset);
    if (keypoint_count != handpose_det::kHandPoseKeypointCount) {
      throw std::runtime_error("Wire handpose keypoint count mismatch");
    }

    auto frame = WireHandposeFrame{};
    frame.metadata.sequence_number = read_u64(payload, offset);
    frame.metadata.timestamp_ns = read_u64(payload, offset);
    frame.metadata.source_sequence_number = read_u64(payload, offset);
    frame.metadata.source_timestamp_ns = read_u64(payload, offset);
    frame.metadata.image_width = read_u32(payload, offset);
    frame.metadata.image_height = read_u32(payload, offset);
    frame.metadata.model_width = read_u32(payload, offset);
    frame.metadata.model_height = read_u32(payload, offset);
    frame.metadata.detection_count = detection_count;
    frame.metadata.keypoint_count = keypoint_count;
    frame.metadata.payload_count = detection_count;

    frame.detections.resize(detection_count);
    for (auto& detection : frame.detections) {
      detection.present = read_u8(payload, offset) != 0;
      detection.is_left_hand = read_u8(payload, offset) != 0;
      detection.class_id = read_u16(payload, offset);
      detection.confidence = read_f32(payload, offset);
      detection.presence_confidence = read_f32(payload, offset);
      detection.box.left = read_f32(payload, offset);
      detection.box.top = read_f32(payload, offset);
      detection.box.right = read_f32(payload, offset);
      detection.box.bottom = read_f32(payload, offset);
      for (auto& keypoint : detection.keypoints) {
        keypoint.x = read_f32(payload, offset);
        keypoint.y = read_f32(payload, offset);
        keypoint.z = read_f32(payload, offset);
        keypoint.confidence = read_f32(payload, offset);
      }
    }

    if (offset != payload.size()) {
      throw std::runtime_error("Wire handpose payload has trailing bytes");
    }

    return frame;
  }

} // namespace signlang::signlang_manager
