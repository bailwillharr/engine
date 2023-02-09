#include "config.h"

#include "game.hpp"

#include "logger.hpp"
#include "window.hpp"

#include <exception>

int main(int argc, char* argv[])
{

	bool enableFrameLimiter = true;

	if (argc == 2) {
		const std::string arg { argv[1] };
		if (arg == "nofpslimit") enableFrameLimiter = false;
	}

	engine::setupLog(PROJECT_NAME);

	INFO("{} v{}", PROJECT_NAME, PROJECT_VERSION);

	try {
		playGame(enableFrameLimiter);
	}
	catch (const std::exception& e) {

		CRITICAL("{}", e.what());

		engine::Window::errorBox(e.what());
#ifndef NDEBUG
		fputs(e.what(), stderr);
		fputc('\n', stderr);
#endif

		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
