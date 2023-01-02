#include "config.h"

#include "application.hpp"
#include "window.hpp"
#include "scene_manager.hpp"
#include "scene.hpp"
#include "components/transform.hpp"
#include "components/renderable.hpp"
#include "systems/transform.hpp"

void playGame()
{
	engine::Application app(PROJECT_NAME, PROJECT_VERSION);

	// configure window
	app.window()->setRelativeMouseMode(true);

	auto myScene = app.sceneManager()->createEmptyScene();

	myScene->createEntity("entity1");

	auto entity1 = myScene->getEntity("entity1");

	myScene->getComponent<engine::TransformComponent>(entity1)->tag = "HELLO WORLD";

	myScene->addComponent<engine::RenderableComponent>(entity1);

	app.gameLoop();
}
