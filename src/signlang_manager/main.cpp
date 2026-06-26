#include "bluetooth_gatt_server.hpp"
#include "common/runtime.hpp"
#include "iceoryx_gateway.hpp"
#include "manager_service.hpp"
#include "program_options.hpp"
#include "spdlog/spdlog.h"

#include <chrono>

namespace {

  auto steady_timestamp_ns() -> std::uint64_t {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
  }

} // namespace

auto main(int argc, char** argv) -> int {
  using signlang::signlang_manager::BluetoothGattOptions;
  using signlang::signlang_manager::BluetoothGattServer;
  using signlang::signlang_manager::IpcHandposeSubscriber;
  using signlang::signlang_manager::ManagerService;
  using signlang::signlang_manager::parse_program_options;

  return signlang::runtime::run_module(argc, argv, parse_program_options, [&](const auto& options) {
    spdlog::info("Starting sign language manager");
    spdlog::info("Prototype database: {}", options.prototypes_path);

    auto manager = ManagerService{options};
    auto bluetooth = BluetoothGattServer{BluetoothGattOptions{
        .adapter_path = options.adapter_path,
        .local_name = options.bluetooth_name,
        .max_notify_payload = options.max_notify_payload,
    }};
    bluetooth.start([&manager](const auto& request) { return manager.handle_packet_bytes(request); });

    auto subscriber = IpcHandposeSubscriber{options.input_service_name, options.subscriber_buffer_size};
    auto next_stream_time_ns = std::uint64_t{0};

    while (!signlang::runtime::shutdown_requested() && subscriber.wait_for_work()) {
      subscriber.receive_latest([&](const auto& metadata, const auto* detections, auto count) {
        const auto now_ns = steady_timestamp_ns();
        if (!manager.streaming_enabled() || !bluetooth.notifications_enabled() || now_ns < next_stream_time_ns) {
          return;
        }

        const auto packet = manager.build_stream_packet(metadata, detections, count);
        bluetooth.notify_packet(packet);
        next_stream_time_ns = now_ns + manager.stream_interval_ns();
      });
    }

    bluetooth.stop();
    return 0;
  });
}
