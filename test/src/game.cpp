#include "config.h"

#include "camera_controller.hpp"
#include "meshgen.hpp"

#include "application.hpp"
#include "window.hpp"
#include "input_manager.hpp"
#include "scene_manager.hpp"
#include "scene.hpp"

#include "components/transform.hpp"
#include "components/collider.hpp"
#include "components/renderable.hpp"

#include "systems/transform.hpp"
#include "systems/render.hpp"

#include "resources/material.hpp"
#include "resources/texture.hpp"

#include "util/model_loader.hpp"

static void configureInputs(engine::InputManager* inputManager)
{
	// user interface mappings
	inputManager->addInputButton("fullscreen", engine::inputs::Key::K_F11);
	// game buttons
	inputManager->addInputButton("fire", engine::inputs::MouseButton::M_LEFT);
	inputManager->addInputButton("aim", engine::inputs::MouseButton::M_RIGHT);
	inputManager->addInputButton("jump", engine::inputs::Key::K_SPACE);
	inputManager->addInputButton("sprint", engine::inputs::Key::K_LSHIFT);
	// game movement
	inputManager->addInputButtonAsAxis("movex", engine::inputs::Key::K_D, engine::inputs::Key::K_A);
	inputManager->addInputButtonAsAxis("movey", engine::inputs::Key::K_W, engine::inputs::Key::K_S);
	// looking around
	inputManager->addInputAxis("lookx", engine::inputs::MouseAxis::X);
	inputManager->addInputAxis("looky", engine::inputs::MouseAxis::Y);
}

void playGame()
{
	engine::Application app(PROJECT_NAME, PROJECT_VERSION);

	app.setFrameLimiter(true);

	// configure window
	app.window()->setRelativeMouseMode(true);
	
	configureInputs(app.inputManager());

	auto myScene = app.sceneManager()->createEmptyScene();

	myScene->registerComponent<CameraControllerComponent>();
	myScene->registerSystem<CameraControllerSystem>();

	auto camera = myScene->createEntity("camera");
	myScene->getComponent<engine::TransformComponent>(camera)->position.y = 8.0f;
	auto cameraCollider = myScene->addComponent<engine::ColliderComponent>(camera);
	cameraCollider->colliderType = engine::ColliderType::SPHERE;
	cameraCollider->colliders.sphereCollider.r = 1.8f;
	myScene->addComponent<CameraControllerComponent>(camera);

	myScene->getSystem<engine::RenderSystem>()->setCameraEntity(camera);

//	engine::util::loadMeshFromFile(myScene, app.getResourcePath("models/van/van.dae"));

	auto grassTexture = std::make_shared<engine::resources::Texture>(
		app.gfx(),
		app.getResourcePath("textures/grass.jpg"),
		engine::resources::Texture::Filtering::ANISOTROPIC,
		true,
		true
	);

	uint32_t enemy = myScene->createEntity("enemy");
	auto enemyRenderable = myScene->addComponent<engine::RenderableComponent>(enemy);
	enemyRenderable->material = std::make_unique<engine::resources::Material>(app.getResource<engine::resources::Shader>("engine.textured"));
	enemyRenderable->material->m_texture = app.getResource<engine::resources::Texture>("engine.white");
	enemyRenderable->mesh = genSphereMesh(app.gfx(), 5.0f, 50, false);
	auto enemyT = myScene->getComponent<engine::TransformComponent>(enemy);
	enemyT->position.x += 10.0f;
	enemyT->position.y += 2.0f;
	enemyT->position.z += 14.0f;
	auto enemyCollider = myScene->addComponent<engine::ColliderComponent>(enemy);
	enemyCollider->colliderType = engine::ColliderType::SPHERE;
	enemyCollider->colliders.sphereCollider.r = 5.0f;

	uint32_t sphere = myScene->createEntity("sphere");
	auto sphereRenderable = myScene->addComponent<engine::RenderableComponent>(sphere);
	sphereRenderable->material = std::make_unique<engine::resources::Material>(app.getResource<engine::resources::Shader>("engine.textured"));
	sphereRenderable->material->m_texture = app.getResource<engine::resources::Texture>("engine.white");
	sphereRenderable->mesh = genSphereMesh(app.gfx(), 100.0f, 50, true);

	uint32_t light = myScene->createEntity("light");
	myScene->getComponent<engine::TransformComponent>(light)->position = glm::vec3{-10.0f, 10.0f, 10.0f};
	auto lightRenderable = myScene->addComponent<engine::RenderableComponent>(light);
	lightRenderable->material = sphereRenderable->material;
	lightRenderable->mesh = genSphereMesh(app.gfx(), 0.5f, 40, false, true);

	uint32_t floor = myScene->createEntity("floor");
	myScene->getComponent<engine::TransformComponent>(floor)->position = glm::vec3{-50.0f, -0.1f, -50.0f};
	auto floorRenderable = myScene->addComponent<engine::RenderableComponent>(floor);
	floorRenderable->material = std::make_shared<engine::resources::Material>(*sphereRenderable->material);
	floorRenderable->material->m_texture = grassTexture;
	floorRenderable->mesh = genCuboidMesh(app.gfx(), 100.0f, 0.1f, 100.0f);
	myScene->addComponent<engine::ColliderComponent>(floor)->colliderType = engine::ColliderType::PLANE;

	app.gameLoop();

}
