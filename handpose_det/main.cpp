#include "program_options.hpp"

#include <exception>
#include <iostream>
#include <variant>

auto main(int argc, char** argv) -> int {
  using signlang::handpose_det::parse_program_options;
  using signlang::handpose_det::ProgramOptions;
  using signlang::handpose_det::ProgramUsage;

  try {
    const auto parse_result = parse_program_options(argc, argv);
    if (const auto* usage = std::get_if<ProgramUsage>(&parse_result); usage != nullptr) {
      std::cout << usage->text << '\n';
      return 0;
    }

    const auto& options = std::get<ProgramOptions>(parse_result);
    std::cout << "handpose detector configured: input=" << options.input_service_name
              << " output=" << options.output_service_name << " model=" << options.model_path << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
