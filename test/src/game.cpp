#include "config.h"

#include "application.hpp"
#include "window.hpp"
#include "scene_manager.hpp"
#include "scene.hpp"
#include "components/mesh_renderer.hpp"

void playGame()
{
	engine::Application app(PROJECT_NAME, PROJECT_VERSION);

	// configure window
	app.window()->setRelativeMouseMode(false);

	auto myScene = std::make_unique<engine::Scene>();

	auto entity1 = myScene->createEntity();

	myScene->m_renderSystem->m_components.emplace(entity1, engine::components::MeshRenderer());

	app.sceneManager()->createScene(std::move(myScene));

	app.gameLoop();
}
