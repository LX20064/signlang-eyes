#include "audio_ring_buffer.hpp"
#include "iceoryx_gateway.hpp"
#include "program_options.hpp"
#include "speech_asr_result.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <exception>
#include <iostream>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>
#include <variant>

namespace {

  volatile std::sig_atomic_t g_should_stop = 0;

  void handle_shutdown_signal(int /* signal_number */) { g_should_stop = 1; }

  void install_signal_handlers() {
    std::signal(SIGINT, handle_shutdown_signal);
    std::signal(SIGTERM, handle_shutdown_signal);
  }

  auto ring_capacity_samples(std::uint64_t window_sample_count, std::uint64_t hop_sample_count) -> std::uint64_t {
    const auto minimum_capacity = window_sample_count + std::max(window_sample_count, hop_sample_count);
    const auto one_second = static_cast<std::uint64_t>(signlang::speech_asr::kWhisperSampleRateHz);
    return std::max(minimum_capacity, window_sample_count + one_second);
  }

} // namespace

auto main(int argc, char** argv) -> int {
  using signlang::speech_asr::AudioRingBuffer;
  using signlang::speech_asr::AudioWindow;
  using signlang::speech_asr::hop_samples_for_overlap;
  using signlang::speech_asr::IpcAudioSubscriber;
  using signlang::speech_asr::kWhisperSampleRateHz;
  using signlang::speech_asr::parse_program_options;
  using signlang::speech_asr::ProgramOptions;
  using signlang::speech_asr::ProgramUsage;
  using signlang::speech_asr::samples_for_window_ms;

  try {
    const auto parse_result = parse_program_options(argc, argv);
    if (const auto* usage = std::get_if<ProgramUsage>(&parse_result); usage != nullptr) {
      std::cout << usage->text << '\n';
      return 0;
    }

    const auto options = std::get<ProgramOptions>(parse_result);
    install_signal_handlers();

    const auto window_sample_count = samples_for_window_ms(kWhisperSampleRateHz, options.window_ms);
    const auto hop_sample_count = hop_samples_for_overlap(window_sample_count, options.overlap_ratio);
    if (window_sample_count == 0) {
      throw std::runtime_error("ASR window has no samples");
    }

    AudioRingBuffer audio_buffer{ring_capacity_samples(window_sample_count, hop_sample_count)};
    std::atomic_bool should_stop{false};
    std::exception_ptr worker_error = nullptr;
    std::mutex worker_error_mutex;

    auto record_worker_error = [&](std::exception_ptr error) {
      {
        const std::lock_guard<std::mutex> lock{worker_error_mutex};
        if (worker_error == nullptr) {
          worker_error = error;
        }
      }
      should_stop.store(true);
      audio_buffer.notify_stop();
    };

    std::thread receiver_thread{[&] {
      try {
        IpcAudioSubscriber audio_subscriber{options.audio_service_name, options.subscriber_buffer_size};
        const auto poll_period = std::chrono::milliseconds(options.poll_period_ms);

        while (!should_stop.load()) {
          const auto receive_stats = audio_subscriber.receive_available(audio_buffer);
          if (receive_stats.accepted_count == 0) {
            std::this_thread::sleep_for(poll_period);
          }
        }
      } catch (...) {
        record_worker_error(std::current_exception());
      }
    }};

    std::thread detector_thread{[&] {
      try {
        AudioWindow audio_window;
        std::optional<std::uint64_t> next_window_start_sample;

        while (audio_buffer.wait_for_window(next_window_start_sample, window_sample_count, hop_sample_count,
                                            should_stop, audio_window)) {
          next_window_start_sample = audio_window.start_sample_index + hop_sample_count;
        }
      } catch (...) {
        record_worker_error(std::current_exception());
      }
    }};

    while (!should_stop.load() && g_should_stop == 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    should_stop.store(true);
    audio_buffer.notify_stop();

    receiver_thread.join();
    detector_thread.join();

    if (worker_error != nullptr) {
      std::rethrow_exception(worker_error);
    }

    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
