#include "config.h"
#include "game.hpp"

// engine
#include "logger.hpp"
#include "window.hpp"

// standard library
#include <exception>
#include <string.h>

int main(int argc, char* argv[])
{

	bool enableFrameLimiter = true;
	if (argc == 2) {
		if (strcmp(argv[1], "nofpslimit") == 0) enableFrameLimiter = false;
	}

	engine::setupLog(PROJECT_NAME);

	LOG_INFO("{} v{}", PROJECT_NAME, PROJECT_VERSION);

	try {
		playGame(enableFrameLimiter);
	}
	catch (const std::exception& e) {
		LOG_CRITICAL("{}", e.what());
		engine::Window::errorBox(e.what());
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
