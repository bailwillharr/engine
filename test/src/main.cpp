#include "config.h"
#include "game.hpp"

// engine
#include "logger.hpp"
#include "window.hpp"

// standard library
#include <exception>
#include <unordered_set>
#include <string.h>

int main(int argc, char* argv[])
{

	GameSettings settings{};
	settings.enableFrameLimiter = true;
	settings.enableValidation = false;
	if (argc >= 2) {
		std::unordered_set<std::string> args{};
		for (int i = 1; i < argc; i++) {
			args.insert(std::string(argv[i]));
		}
		if (args.contains("nofpslimit")) settings.enableFrameLimiter = false;
		if (args.contains("gpuvalidation")) settings.enableValidation = true;
	}

	engine::setupLog(PROJECT_NAME);

	LOG_INFO("{} v{}", PROJECT_NAME, PROJECT_VERSION);

	try {
		playGame(settings);
	}
	catch (const std::exception& e) {
		LOG_CRITICAL("{}", e.what());
		engine::Window::errorBox(e.what());
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
