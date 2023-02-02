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

	app.setFrameLimiter(false);

	// configure window
	app.window()->setRelativeMouseMode(true);
	
	configureInputs(app.inputManager());

	auto myScene = app.sceneManager()->createEmptyScene();

	/* create camera */
	{
		myScene->registerComponent<CameraControllerComponent>();
		myScene->registerSystem<CameraControllerSystem>();

		auto camera = myScene->createEntity("camera");
		myScene->getComponent<engine::TransformComponent>(camera)->position.y = 8.0f;
		auto cameraCollider = myScene->addComponent<engine::ColliderComponent>(camera);
		cameraCollider->isStatic = false;
		cameraCollider->aabb = { { -0.2f, -1.5f, -0.2f }, { 0.2f, 0.2f, 0.2f} }; // Origin is at eye level
		myScene->addComponent<CameraControllerComponent>(camera);

		myScene->getSystem<engine::RenderSystem>()->setCameraEntity(camera);
	}

	/* shared resources */
	auto grassTexture = std::make_shared<engine::resources::Texture>(
		app.gfx(),
		app.getResourcePath("textures/grass.jpg"),
		engine::resources::Texture::Filtering::ANISOTROPIC,
		true,
		true
	);
	auto spaceTexture = std::make_shared<engine::resources::Texture>(
		app.gfx(),
		app.getResourcePath("textures/space2.png"),
		engine::resources::Texture::Filtering::ANISOTROPIC,
		true,
		true
	);

	/* skybox */
	{
		uint32_t skybox = myScene->createEntity("skybox");
		auto skyboxRenderable = myScene->addComponent<engine::RenderableComponent>(skybox);
		skyboxRenderable->material = std::make_unique<engine::resources::Material>(app.getResource<engine::resources::Shader>("engine.skybox"));
		skyboxRenderable->material->m_texture = spaceTexture;
//		skyboxRenderable->mesh = genSphereMesh(app.gfx(), 1.0f, 50, true);
		skyboxRenderable->mesh = genCuboidMesh(app.gfx(), 2.0f, 2.0f, 2.0f, 1.0f, true);
		myScene->getComponent<engine::TransformComponent>(skybox)->position = { -1.0f, -1.0f, -1.0f };
	}

	/* enemy sphere */
	{
		uint32_t enemy = myScene->createEntity("enemy");
		auto enemyRenderable = myScene->addComponent<engine::RenderableComponent>(enemy);
		enemyRenderable->material = std::make_unique<engine::resources::Material>(app.getResource<engine::resources::Shader>("engine.textured"));
		enemyRenderable->material->m_texture = app.getResource<engine::resources::Texture>("engine.white");
		enemyRenderable->mesh = genSphereMesh(app.gfx(), 5.0f, 50, false);
		auto enemyTransform = myScene->getComponent<engine::TransformComponent>(enemy);
		enemyTransform->position.x = 10.0f;
		enemyTransform->position.y = 0.0f;
		enemyTransform->position.z = 14.0f;
		auto enemyCollider = myScene->addComponent<engine::ColliderComponent>(enemy);
		enemyCollider->isStatic = true;
		enemyCollider->aabb = { { -5.0f, -5.0f, -5.0f }, { 5.0f, 5.0f, 5.0f } }; // A box enclosing the sphere
	}

	/* floor */
	{
		uint32_t floor = myScene->createEntity("floor");
		myScene->getComponent<engine::TransformComponent>(floor)->position = glm::vec3{-50.0f, -0.1f, -50.0f};
		auto floorRenderable = myScene->addComponent<engine::RenderableComponent>(floor);
		floorRenderable->material = std::make_shared<engine::resources::Material>(app.getResource<engine::resources::Shader>("engine.textured"));
		floorRenderable->material->m_texture = grassTexture;
		floorRenderable->mesh = genCuboidMesh(app.gfx(), 100.0f, 0.1f, 100.0f, 128.0f);
		auto floorCollider = myScene->addComponent<engine::ColliderComponent>(floor);
		floorCollider->isStatic = true;
		floorCollider->aabb = { { 0.0f, 0.0f, 0.0f }, { 100.0f, 0.1f, 100.0f } };
	}

	app.gameLoop();

}
