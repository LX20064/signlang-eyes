#include "audio_publisher.hpp"

#include "alsa_capture_device.hpp"

#include <new>
#include <stdexcept>
#include <string>
#include <utility>

namespace signlang::audio_frontend {

AudioPublisher::AudioPublisher(const std::string& service_name)
    : node_ { create_node() }
    , publisher_ { create_publisher(node_, service_name) } {
}

void AudioPublisher::capture_and_publish(AlsaCaptureDevice& capture_device, std::uint64_t sequence_number) {
    auto loan_result = publisher_.loan_uninit();
    if (!loan_result.has_value()) {
        throw std::runtime_error("Failed to loan iceoryx2 audio frame sample");
    }

    auto loaned_sample = std::move(loan_result.value());
    // Construct the payload in loaned shared memory so ALSA can write into it directly.
    auto* frame = ::new (static_cast<void*>(&loaned_sample.payload_mut())) AudioFrame;
    capture_device.capture_frame(*frame, sequence_number);

    auto initialized_sample = iox2::assume_init(std::move(loaned_sample));
    const auto send_result = iox2::send(std::move(initialized_sample));
    if (!send_result.has_value()) {
        throw std::runtime_error("Failed to publish audio frame through iceoryx2");
    }
}

auto AudioPublisher::create_node() -> iox2::Node<iox2::ServiceType::Ipc> {
    iox2::set_log_level_from_env_or(iox2::LogLevel::Warn);

    auto node = iox2::NodeBuilder().create<iox2::ServiceType::Ipc>();
    if (!node.has_value()) {
        throw std::runtime_error("Failed to create iceoryx2 IPC node");
    }

    return std::move(node.value());
}

auto AudioPublisher::create_publisher(const iox2::Node<iox2::ServiceType::Ipc>& node,
                                      const std::string& service_name)
    -> iox2::Publisher<iox2::ServiceType::Ipc, AudioFrame, void> {
    const auto parsed_service_name = iox2::ServiceName::create(service_name.c_str());
    if (!parsed_service_name.has_value()) {
        throw std::runtime_error("Invalid iceoryx2 service name: " + service_name);
    }

    auto service = node.service_builder(parsed_service_name.value())
                       .publish_subscribe<AudioFrame>()
                       .open_or_create();
    if (!service.has_value()) {
        throw std::runtime_error("Failed to open or create iceoryx2 service: " + service_name);
    }

    auto publisher = service.value().publisher_builder().create();
    if (!publisher.has_value()) {
        throw std::runtime_error("Failed to create iceoryx2 publisher for service: " + service_name);
    }

    return std::move(publisher.value());
}

} // namespace signlang::audio_frontend
