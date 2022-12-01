#include "config.h"

#include "application.hpp"
#include "window.hpp"

void playGame()
{
	engine::Application app(PROJECT_NAME, PROJECT_VERSION);

	// configure window
	app.window()->setRelativeMouseMode(false);

	app.gameLoop();
}
