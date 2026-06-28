#ifndef SIGNLANG_EYES_SIGNLANG_MANAGER_BLUETOOTH_GATT_SERVER_HPP
#define SIGNLANG_EYES_SIGNLANG_MANAGER_BLUETOOTH_GATT_SERVER_HPP

#include <gio/gio.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace signlang::signlang_manager {

  struct BluetoothGattOptions {
    std::string adapter_path;
    std::string local_name;
    std::uint32_t max_notify_payload;
  };

  class BluetoothGattServer {
  public:
    using PacketHandler = std::function<std::vector<std::uint8_t>(const std::vector<std::uint8_t>&)>;

    explicit BluetoothGattServer(BluetoothGattOptions options);
    ~BluetoothGattServer();

    BluetoothGattServer(const BluetoothGattServer&) = delete;
    auto operator=(const BluetoothGattServer&) -> BluetoothGattServer& = delete;
    BluetoothGattServer(BluetoothGattServer&&) = delete;
    auto operator=(BluetoothGattServer&&) -> BluetoothGattServer& = delete;

    void start(PacketHandler handler);
    void stop();
    [[nodiscard]] auto request_start_notify(const char* sender) -> bool;
    void request_stop_notify(const char* sender);
    void release_notify_owner_if_matches(const char* owner_name);
    void handle_write_value(const std::vector<std::uint8_t>& value);
    void notify_packet(const std::vector<std::uint8_t>& packet);
    [[nodiscard]] auto notifications_enabled() const -> bool;
    [[nodiscard]] auto max_notify_payload() const -> std::uint32_t;
    [[nodiscard]] auto local_name() const -> const std::string&;

  private:
    void register_objects();
    void unregister_objects();
    [[nodiscard]] auto read_adapter_powered() -> bool;
    void set_adapter_powered(bool powered);
    void reset_adapter();
    void ensure_adapter_powered();
    void register_with_bluez();
    void unregister_from_bluez();
    void run_loop();
    void emit_tx_value_changed(const std::vector<std::uint8_t>& value);
    void release_local_resources();
    void close_connection();

    BluetoothGattOptions options_;
    GDBusConnection* connection_{nullptr};
    GMainLoop* loop_{nullptr};
    GDBusNodeInfo* object_manager_node_{nullptr};
    GDBusNodeInfo* service_node_{nullptr};
    GDBusNodeInfo* characteristic_node_{nullptr};
    GDBusNodeInfo* advertisement_node_{nullptr};
    std::vector<unsigned int> object_registration_ids_;
    unsigned int name_owner_watch_id_{0};
    bool gatt_registered_{false};
    bool advertisement_registered_{false};
    std::thread loop_thread_;
    PacketHandler handler_;
    mutable std::mutex mutex_;
    std::string notify_owner_;
    std::atomic_bool notifications_enabled_{false};
    std::atomic_bool started_{false};
  };

} // namespace signlang::signlang_manager

#endif // SIGNLANG_EYES_SIGNLANG_MANAGER_BLUETOOTH_GATT_SERVER_HPP
