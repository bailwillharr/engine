#pragma once

#include <filesystem>

#include "log.hpp"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

namespace engine {

	struct AppInfo {
		const char* name;
		const char* version;
	};


	bool versionFromCharArray(const char* version, int* major, int* minor, int* patch);

}
