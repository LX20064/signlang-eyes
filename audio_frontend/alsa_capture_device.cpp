#include "alsa_capture_device.hpp"

#include <chrono>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>

namespace signlang::audio_frontend {
namespace {

struct HardwareParamsDeleter {
    void operator()(snd_pcm_hw_params_t* hardware_params) const noexcept {
        snd_pcm_hw_params_free(hardware_params);
    }
};

using HardwareParams = std::unique_ptr<snd_pcm_hw_params_t, HardwareParamsDeleter>;

auto alsa_error_message(const std::string& context, int error_code) -> std::string {
    return context + ": " + snd_strerror(error_code);
}

auto create_hardware_params() -> HardwareParams {
    snd_pcm_hw_params_t* hardware_params = nullptr;
    const auto allocation_result = snd_pcm_hw_params_malloc(&hardware_params);
    if (allocation_result < 0) {
        throw std::runtime_error(alsa_error_message("Failed to allocate ALSA hardware parameters",
                                                    allocation_result));
    }

    return HardwareParams { hardware_params };
}

auto frames_for_period(std::uint32_t publish_period_ms) -> snd_pcm_uframes_t {
    if (publish_period_ms == 0 || publish_period_ms > kMaxPublishPeriodMs) {
        throw std::runtime_error("Invalid ALSA capture period");
    }

    return static_cast<snd_pcm_uframes_t>(publish_period_ms * kFramesPerMillisecond);
}

auto steady_timestamp_ns() -> std::uint64_t {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
}

} // namespace

AlsaCaptureDevice::AlsaCaptureDevice(const std::string& device_name, std::uint32_t publish_period_ms)
    : device_name_ { device_name }
    , publish_period_ms_ { publish_period_ms }
    , frames_per_packet_ { frames_for_period(publish_period_ms) } {
    snd_pcm_t* pcm_handle = nullptr;
    const auto open_result = snd_pcm_open(&pcm_handle, device_name_.c_str(), SND_PCM_STREAM_CAPTURE, 0);
    if (open_result < 0) {
        throw std::runtime_error(alsa_error_message("Failed to open ALSA capture device '" + device_name_ + "'",
                                                    open_result));
    }

    pcm_handle_.reset(pcm_handle);
    configure();
}

void AlsaCaptureDevice::PcmHandleDeleter::operator()(snd_pcm_t* handle) const noexcept {
    if (handle != nullptr) {
        snd_pcm_close(handle);
    }
}

void AlsaCaptureDevice::capture_frame(AudioFrame& frame, std::uint64_t sequence_number) {
    frame.sequence_number = sequence_number;
    frame.timestamp_ns = steady_timestamp_ns();
    frame.sample_rate_hz = kSampleRateHz;
    frame.publish_period_ms = publish_period_ms_;
    frame.frame_count = static_cast<std::uint32_t>(frames_per_packet_);
    frame.channel_count = kChannelCount;
    frame.bits_per_sample = kBitsPerSample;

    snd_pcm_uframes_t frames_read = 0;
    while (frames_read < frames_per_packet_) {
        auto* write_position = frame.samples.data() + static_cast<std::size_t>(frames_read * kChannelCount);
        const auto frames_remaining = frames_per_packet_ - frames_read;
        const auto read_result = snd_pcm_readi(pcm_handle_.get(), write_position, frames_remaining);

        if (read_result < 0) {
            recover_from_read_error(static_cast<int>(read_result));
            continue;
        }

        if (read_result == 0) {
            throw std::runtime_error("ALSA capture stream returned no frames");
        }

        frames_read += static_cast<snd_pcm_uframes_t>(read_result);
    }
}

void AlsaCaptureDevice::configure() {
    auto hardware_params = create_hardware_params();

    auto result = snd_pcm_hw_params_any(pcm_handle_.get(), hardware_params.get());
    if (result < 0) {
        throw std::runtime_error(alsa_error_message("Failed to initialize ALSA hardware parameters", result));
    }

    result = snd_pcm_hw_params_set_access(pcm_handle_.get(), hardware_params.get(), SND_PCM_ACCESS_RW_INTERLEAVED);
    if (result < 0) {
        throw std::runtime_error(alsa_error_message("Failed to set ALSA access mode", result));
    }

    result = snd_pcm_hw_params_set_format(pcm_handle_.get(), hardware_params.get(), SND_PCM_FORMAT_S16_LE);
    if (result < 0) {
        throw std::runtime_error(alsa_error_message("Failed to set ALSA sample format", result));
    }

    unsigned int sample_rate_hz = kSampleRateHz;
    result = snd_pcm_hw_params_set_rate_near(pcm_handle_.get(), hardware_params.get(), &sample_rate_hz, nullptr);
    if (result < 0 || sample_rate_hz != kSampleRateHz) {
        throw std::runtime_error("Failed to set ALSA sample rate to " + std::to_string(kSampleRateHz) + " Hz");
    }

    result = snd_pcm_hw_params_set_channels(pcm_handle_.get(), hardware_params.get(), kChannelCount);
    if (result < 0) {
        throw std::runtime_error(alsa_error_message("Failed to set ALSA channel count", result));
    }

    snd_pcm_uframes_t period_size = frames_per_packet_;
    result = snd_pcm_hw_params_set_period_size_near(pcm_handle_.get(), hardware_params.get(), &period_size, nullptr);
    if (result < 0) {
        throw std::runtime_error(alsa_error_message("Failed to set ALSA period size", result));
    }

    result = snd_pcm_hw_params(pcm_handle_.get(), hardware_params.get());
    if (result < 0) {
        throw std::runtime_error(alsa_error_message("Failed to apply ALSA hardware parameters", result));
    }

    result = snd_pcm_prepare(pcm_handle_.get());
    if (result < 0) {
        throw std::runtime_error(alsa_error_message("Failed to prepare ALSA capture device", result));
    }
}

void AlsaCaptureDevice::recover_from_read_error(int error_code) {
    const auto recover_result = snd_pcm_recover(pcm_handle_.get(), error_code, 1);
    if (recover_result < 0) {
        throw std::runtime_error(alsa_error_message("Failed to recover ALSA capture stream", recover_result));
    }
}

} // namespace signlang::audio_frontend
