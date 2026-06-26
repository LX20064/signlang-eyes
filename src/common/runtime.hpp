#ifndef SIGNLANG_EYES_COMMON_RUNTIME_HPP
#define SIGNLANG_EYES_COMMON_RUNTIME_HPP

#include "common/logging.hpp"

#include <csignal>
#include <exception>
#include <iostream>
#include <type_traits>
#include <utility>
#include <variant>

namespace signlang::runtime {

  namespace detail {

    template <typename T>
    concept UsageResult = requires(const T& value) {
      { value.text };
    };

    template <typename T>
    concept RuntimeOptions = requires(const T& value) {
      { value.logging };
    };

  } // namespace detail

  inline volatile std::sig_atomic_t g_shutdown_requested = 0;

  inline void request_shutdown(int /* signal_number */) { g_shutdown_requested = 1; }

  inline void install_shutdown_signal_handlers() {
    g_shutdown_requested = 0;
    std::signal(SIGINT, request_shutdown);
    std::signal(SIGTERM, request_shutdown);
  }

  inline auto shutdown_requested() -> bool { return g_shutdown_requested != 0; }

  template <typename ParseOptions, typename RunModule>
  auto run_module(int argc, char** argv, ParseOptions&& parse_options, RunModule&& run_module) -> int {
    signlang::logging::initialize();

    try {
      auto parse_result = std::forward<ParseOptions>(parse_options)(argc, argv);
      auto exit_code = 0;

      std::visit(
          [&](const auto& result) {
            using Result = std::remove_cvref_t<decltype(result)>;
            if constexpr (detail::UsageResult<Result>) {
              std::cout << result.text << '\n';
            } else {
              static_assert(detail::RuntimeOptions<Result>, "Runtime options must expose a logging field");
              signlang::logging::initialize(result.logging);
              install_shutdown_signal_handlers();
              exit_code = std::forward<RunModule>(run_module)(result);
            }
          },
          parse_result);

      return exit_code;
    } catch (const std::exception& error) {
      spdlog::error("{}", error.what());
      return 1;
    }
  }

} // namespace signlang::runtime

#endif // SIGNLANG_EYES_COMMON_RUNTIME_HPP
