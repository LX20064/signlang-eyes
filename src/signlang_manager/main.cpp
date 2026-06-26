#include "bluetooth_gatt_server.hpp"
#include "common/logging.hpp"
#include "iceoryx_gateway.hpp"
#include "manager_service.hpp"
#include "program_options.hpp"
#include "spdlog/spdlog.h"

#include <chrono>
#include <csignal>
#include <exception>
#include <iostream>
#include <thread>
#include <variant>

namespace {

  volatile std::sig_atomic_t g_should_stop = 0;

  void handle_shutdown_signal(int) { g_should_stop = 1; }

  void install_signal_handlers() {
    std::signal(SIGINT, handle_shutdown_signal);
    std::signal(SIGTERM, handle_shutdown_signal);
  }

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
  using signlang::signlang_manager::ProgramOptions;
  using signlang::signlang_manager::ProgramUsage;

  signlang::logging::initialize();

  try {
    const auto parse_result = parse_program_options(argc, argv);
    if (const auto* usage = std::get_if<ProgramUsage>(&parse_result); usage != nullptr) {
      std::cout << usage->text << '\n';
      return 0;
    }

    const auto options = std::get<ProgramOptions>(parse_result);
    signlang::logging::initialize(options.logging);
    install_signal_handlers();

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

    while (g_should_stop == 0 && subscriber.wait_for_work()) {
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
  } catch (const std::exception& error) {
    spdlog::error("{}", error.what());
    return 1;
  }
}
