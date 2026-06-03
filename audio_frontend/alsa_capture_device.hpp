#ifndef SIGNLANG_EYES_EDGEAI_AUDIO_FRONTEND_ALSA_CAPTURE_DEVICE_HPP
#define SIGNLANG_EYES_EDGEAI_AUDIO_FRONTEND_ALSA_CAPTURE_DEVICE_HPP

#include "audio_frame.hpp"

#include <alsa/asoundlib.h>

#include <memory>
#include <string>

namespace signlang::audio_frontend {

class AlsaCaptureDevice {
  public:
    AlsaCaptureDevice(const std::string& device_name, std::uint32_t publish_period_ms);
    ~AlsaCaptureDevice() = default;

    AlsaCaptureDevice(const AlsaCaptureDevice&) = delete;
    auto operator=(const AlsaCaptureDevice&) -> AlsaCaptureDevice& = delete;
    AlsaCaptureDevice(AlsaCaptureDevice&&) = delete;
    auto operator=(AlsaCaptureDevice&&) -> AlsaCaptureDevice& = delete;

    void capture_frame(AudioFrame& frame, std::uint64_t sequence_number);

  private:
    struct PcmHandleDeleter {
        void operator()(snd_pcm_t* handle) const noexcept;
    };

    using PcmHandle = std::unique_ptr<snd_pcm_t, PcmHandleDeleter>;

    void configure();
    void recover_from_read_error(int error_code);

    PcmHandle pcm_handle_;
    std::string device_name_;
    std::uint32_t publish_period_ms_;
    snd_pcm_uframes_t frames_per_packet_;
};

} // namespace signlang::audio_frontend

#endif // SIGNLANG_EYES_EDGEAI_AUDIO_FRONTEND_ALSA_CAPTURE_DEVICE_HPP
