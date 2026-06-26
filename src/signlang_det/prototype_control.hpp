#ifndef SIGNLANG_EYES_SIGNLANG_DET_PROTOTYPE_CONTROL_HPP
#define SIGNLANG_EYES_SIGNLANG_DET_PROTOTYPE_CONTROL_HPP

#include <array>
#include <cstdint>
#include <type_traits>

namespace signlang::signlang_det {

  constexpr auto kPrototypeControlMessageLength = std::uint32_t{128};

  enum class PrototypeControlCommand : std::uint32_t {
    ReloadPrototypes = 1,
    GetStatus = 2,
  };

  enum class PrototypeControlStatus : std::uint32_t {
    Ok = 0,
    Failed = 1,
    UnsupportedCommand = 2,
  };

  struct PrototypeControlRequest {
    static constexpr const char* IOX2_TYPE_NAME = "signlang_prototype_control_request";

    PrototypeControlCommand command;
    std::uint32_t request_id;
  };

  struct PrototypeControlResponse {
    static constexpr const char* IOX2_TYPE_NAME = "signlang_prototype_control_response";

    PrototypeControlStatus status;
    std::uint32_t request_id;
    std::uint32_t loaded_gesture_count;
    std::uint32_t loaded_sample_count;
    std::array<char, kPrototypeControlMessageLength> message;
  };

  static_assert(std::is_trivially_copyable_v<PrototypeControlRequest>);
  static_assert(std::is_trivially_copyable_v<PrototypeControlResponse>);

} // namespace signlang::signlang_det

#endif // SIGNLANG_EYES_SIGNLANG_DET_PROTOTYPE_CONTROL_HPP
