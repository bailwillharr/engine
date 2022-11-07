#include "config.h"
#include "game.hpp"

#include "logger.hpp"
#include "window.hpp"

#include <exception>

int main(int argc, char *argv[])
{

	engine::setupLog(PROJECT_NAME);

	INFO("{} v{}", PROJECT_NAME, PROJECT_VERSION);

	try {
		playGame();
	}
	catch (const std::exception& e) {

		CRITICAL("{}", e.what());

#ifdef NDEBUG
		engine::Window::errorBox(e.what());
#else
		fputs(e.what(), stderr);
		fputc('\n', stderr);
#endif

		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
