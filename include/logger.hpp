#ifndef ENGINE_INCLUDE_LOGGER_H_
#define ENGINE_INCLUDE_LOGGER_H_

#include <filesystem>
#include <memory>

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "log.hpp"

namespace engine {

// To be executed in the target application, NOT engine.dll
void SetupLog(const char* appName) {
  const std::string LOG_FILENAME{std::string(appName) + ".log"};

#ifdef NDEBUG
  // RELEASE
  const std::filesystem::path log_path{std::filesystem::temp_directory_path() /
                                       LOG_FILENAME};
#else
  // DEBUG
  const std::filesystem::path log_path{LOG_FILENAME};
#endif

  std::vector<spdlog::sink_ptr> sinks;

  sinks.emplace_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(
      log_path.string(), true));
  sinks.back()->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");

  sinks.emplace_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
  sinks.back()->set_pattern("[%H:%M:%S.%e] [%l] %v");

  auto logger =
      std::make_shared<spdlog::logger>(appName, sinks.begin(), sinks.end());

  // Logs below INFO are ignored through macros in release (see log.hpp)
  logger->set_level(spdlog::level::trace);

  spdlog::register_logger(logger);
  spdlog::set_default_logger(logger);
  spdlog::flush_every(std::chrono::seconds(60));

  LOG_INFO("Created log with path: {}", log_path.string());
}

}  // namespace engine

#endif