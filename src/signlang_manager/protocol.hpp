#ifndef SIGNLANG_EYES_SIGNLANG_MANAGER_PROTOCOL_HPP
#define SIGNLANG_EYES_SIGNLANG_MANAGER_PROTOCOL_HPP

#include <cstdint>
#include <vector>

namespace signlang::signlang_manager {

  constexpr auto kProtocolVersion = std::uint8_t{1};
  constexpr auto kPacketHeaderSize = std::uint16_t{24};

  enum class PacketType : std::uint8_t {
    Request = 1,
    Response = 2,
    Event = 3,
    Stream = 4,
  };

  enum class CommandId : std::uint16_t {
    GetCapabilities = 0x0001,
    SetStreamConfig = 0x0101,
    HandposeFrame = 0x0102,
    ListGestures = 0x0201,
    AddGestureBegin = 0x0202,
    AddGestureChunk = 0x0203,
    AddGestureCommit = 0x0204,
    AddGestureAbort = 0x0205,
    DeleteGesture = 0x0206,
    GetStatus = 0x0301,
  };

  struct ProtocolPacket {
    PacketType type{PacketType::Request};
    std::uint16_t command_id{0};
    std::uint32_t request_id{0};
    std::uint16_t flags{0};
    std::vector<std::uint8_t> payload;
  };

  auto encode_packet(const ProtocolPacket& packet) -> std::vector<std::uint8_t>;
  auto decode_packet(const std::vector<std::uint8_t>& bytes) -> ProtocolPacket;
  auto crc32(const std::uint8_t* data, std::size_t size) -> std::uint32_t;

} // namespace signlang::signlang_manager

#endif // SIGNLANG_EYES_SIGNLANG_MANAGER_PROTOCOL_HPP
