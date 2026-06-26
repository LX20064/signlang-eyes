#include "protocol.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

  void require(bool condition, const char* message) {
    if (!condition) {
      throw std::runtime_error(message);
    }
  }

  void test_packet_round_trip() {
    auto packet = signlang::signlang_manager::ProtocolPacket{};
    packet.type = signlang::signlang_manager::PacketType::Request;
    packet.command_id = 0x0201;
    packet.request_id = 42;
    packet.flags = 0x0003;
    packet.payload = {0x11, 0x22, 0x33, 0x44};

    const auto encoded = signlang::signlang_manager::encode_packet(packet);
    const auto decoded = signlang::signlang_manager::decode_packet(encoded);

    require(decoded.type == packet.type, "packet type should round trip");
    require(decoded.command_id == packet.command_id, "command id should round trip");
    require(decoded.request_id == packet.request_id, "request id should round trip");
    require(decoded.flags == packet.flags, "flags should round trip");
    require(decoded.payload == packet.payload, "payload should round trip");
  }

  void test_bad_magic_is_rejected() {
    auto packet = signlang::signlang_manager::ProtocolPacket{};
    packet.type = signlang::signlang_manager::PacketType::Request;
    packet.command_id = 1;
    packet.request_id = 2;
    packet.payload = {0xAA};

    auto encoded = signlang::signlang_manager::encode_packet(packet);
    encoded[0] = 0x00;

    bool rejected = false;
    try {
      static_cast<void>(signlang::signlang_manager::decode_packet(encoded));
    } catch (const std::runtime_error&) {
      rejected = true;
    }

    require(rejected, "bad magic should be rejected");
  }

  void test_payload_crc_is_rejected() {
    auto packet = signlang::signlang_manager::ProtocolPacket{};
    packet.type = signlang::signlang_manager::PacketType::Response;
    packet.command_id = 2;
    packet.request_id = 3;
    packet.payload = {0x01, 0x02, 0x03};

    auto encoded = signlang::signlang_manager::encode_packet(packet);
    encoded.back() ^= 0x7F;

    bool rejected = false;
    try {
      static_cast<void>(signlang::signlang_manager::decode_packet(encoded));
    } catch (const std::runtime_error&) {
      rejected = true;
    }

    require(rejected, "bad crc should be rejected");
  }

} // namespace

auto main() -> int {
  try {
    test_packet_round_trip();
    test_bad_magic_is_rejected();
    test_payload_crc_is_rejected();
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }

  return 0;
}
