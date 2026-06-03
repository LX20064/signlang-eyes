#include "program_options.hpp"

#include "audio_frame.hpp"

#include "cxxopts.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>

namespace signlang::audio_frontend {

auto parse_program_options(int argc, char** argv) -> ProgramOptionsParseResult {
    cxxopts::Options options {
        "signlang_eyes_edgeai_audio_frontend",
        "Capture PCM audio from ALSA and publish it through an iceoryx2 publish-subscribe service."
    };

    options.add_options()
        ("d,device", "ALSA audio device name", cxxopts::value<std::string>())
        ("s,service", "iceoryx2 publish-subscribe service name", cxxopts::value<std::string>())
        ("p,period-ms",
         "Audio publish period in milliseconds",
         cxxopts::value<std::uint32_t>()->default_value(std::to_string(kDefaultPublishPeriodMs)))
        ("h,help", "Print usage");

    const auto parsed_options = options.parse(argc, argv);
    if (parsed_options.count("help") != 0) {
        return ProgramUsage { .text = options.help() };
    }

    if (parsed_options.count("device") == 0 || parsed_options.count("service") == 0) {
        throw std::runtime_error("Both --device and --service are required.\n\n" + options.help());
    }

    const auto publish_period_ms = parsed_options["period-ms"].as<std::uint32_t>();
    if (publish_period_ms == 0 || publish_period_ms > kMaxPublishPeriodMs) {
        throw std::runtime_error(
            "--period-ms must be between 1 and " + std::to_string(kMaxPublishPeriodMs) + ".\n\n" + options.help());
    }

    return ProgramOptionsParseResult { ProgramOptions {
        .audio_device_name = parsed_options["device"].as<std::string>(),
        .service_name = parsed_options["service"].as<std::string>(),
        .publish_period_ms = publish_period_ms,
    } };
}

} // namespace signlang::audio_frontend
