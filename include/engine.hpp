#pragma once

#include <filesystem>

#include "log.hpp"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

namespace engine {

	// To be executed in the target application, NOT engine.dll
	void setupLog(const char* appName)
	{

		const std::string LOG_FILENAME{ std::string(appName) + ".log"};

#ifdef NDEBUG
		// RELEASE
		const std::filesystem::path log_path{ std::filesystem::temp_directory_path() / LOG_FILENAME };
#else
		// DEBUG
		const std::filesystem::path log_path{ LOG_FILENAME };
#endif

		std::vector<spdlog::sink_ptr> sinks;

		sinks.emplace_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_path.string(), true));

#ifndef NDEBUG
		// DEBUG
		sinks.emplace_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
#endif

		auto logger = std::make_shared<spdlog::logger>("sdltest", sinks.begin(), sinks.end());

		logger->set_level(spdlog::level::trace);

		spdlog::register_logger(logger);
		spdlog::set_default_logger(logger);

		INFO("Created log with path: {}", log_path.string());

	}

}