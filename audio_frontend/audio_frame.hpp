#ifndef SIGNLANG_EYES_EDGEAI_AUDIO_FRONTEND_AUDIO_FRAME_HPP
#define SIGNLANG_EYES_EDGEAI_AUDIO_FRONTEND_AUDIO_FRAME_HPP

#include <array>
#include <cstdint>
#include <type_traits>

namespace signlang::audio_frontend {

constexpr std::uint32_t kSampleRateHz = 16'000;
constexpr std::uint32_t kFramesPerMillisecond = kSampleRateHz / 1'000;
constexpr std::uint16_t kChannelCount = 1;
constexpr std::uint16_t kBitsPerSample = 16;
constexpr std::uint32_t kDefaultPublishPeriodMs = 100;
constexpr std::uint32_t kMaxPublishPeriodMs = 1'000;
constexpr std::uint32_t kMaxFramesPerPacket = kFramesPerMillisecond * kMaxPublishPeriodMs;
constexpr std::uint32_t kMaxSamplesPerPacket = kMaxFramesPerPacket * kChannelCount;

static_assert(kSampleRateHz % 1'000 == 0);

struct AudioFrame {
    std::uint64_t sequence_number;
    std::uint64_t timestamp_ns;
    std::uint32_t sample_rate_hz;
    std::uint32_t publish_period_ms;
    std::uint32_t frame_count;
    std::uint16_t channel_count;
    std::uint16_t bits_per_sample;
    std::array<std::int16_t, kMaxSamplesPerPacket> samples;
};

static_assert(std::is_trivially_copyable_v<AudioFrame>);

} // namespace signlang::audio_frontend

#endif // SIGNLANG_EYES_EDGEAI_AUDIO_FRONTEND_AUDIO_FRAME_HPP
