#include "config.h"

#include "application.hpp"
#include "window.hpp"
#include "scene_manager.hpp"
#include "scene.hpp"
#include "ecs/mesh_renderer.hpp"

void playGame()
{
	engine::Application app(PROJECT_NAME, PROJECT_VERSION);

	// configure window
	app.window()->setRelativeMouseMode(false);

	auto myScene = std::make_unique<engine::Scene>();

	auto entity1 = myScene->createEntity();

	auto myTexture = std::make_unique<engine::resources::Texture>(app.gfx(), app.getResourcePath("textures/grass.jpg"));

	app.sceneManager()->getTextureManager()->add("GRASS", std::move(myTexture));
	myScene->m_renderSystem->m_components.emplace(
		entity1,
		engine::ecs::MeshRendererComponent{
			.number = 69,
			.texture = app.sceneManager()->getTextureManager()->get("GRASS"),
		}
	);

	app.sceneManager()->createScene(std::move(myScene));

	app.gameLoop();
}
