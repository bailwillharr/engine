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

#include "game.hpp"

static void configureInputs(engine::InputManager* inputManager)
{
	// user interface mappings
	inputManager->addInputButton("fullscreen", engine::inputs::Key::K_F11);
	inputManager->addInputButton("exit", engine::inputs::Key::K_ESCAPE);
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

void playGame(GameSettings settings)
{
	LOG_INFO("FPS limiter: {}", settings.enableFrameLimiter ? "ON" : "OFF");
	LOG_INFO("Graphics Validation: {}", settings.enableValidation ? "ON" : "OFF");

	engine::gfx::GraphicsSettings graphicsSettings{};
	graphicsSettings.enableValidation = settings.enableValidation;
	graphicsSettings.vsync = true;
	graphicsSettings.waitForPresent = false;
	graphicsSettings.msaaLevel = engine::gfx::MSAALevel::MSAA_OFF;
	engine::Application app(PROJECT_NAME, PROJECT_VERSION, graphicsSettings);

	app.setFrameLimiter(settings.enableFrameLimiter);

	// configure window
	app.window()->setRelativeMouseMode(true);
	
	configureInputs(app.inputManager());

	auto myScene = app.sceneManager()->createEmptyScene();

	/* create camera */
	{
		myScene->registerComponent<CameraControllerComponent>();
		myScene->registerSystem<CameraControllerSystem>();

		auto camera = myScene->createEntity("camera");
		myScene->getComponent<engine::TransformComponent>(camera)->position = { 0.0f, 10.0f, 0.0f };
		auto cameraCollider = myScene->addComponent<engine::ColliderComponent>(camera);
		cameraCollider->isStatic = false;
		cameraCollider->isTrigger = true;
		cameraCollider->aabb = { { -0.2f, -1.5f, -0.2f }, { 0.2f, 0.2f, 0.2f} }; // Origin is at eye level
		myScene->addComponent<CameraControllerComponent>(camera);
		myScene->events()->subscribeToEventType<engine::PhysicsSystem::CollisionEvent>(
				engine::EventSubscriberKind::ENTITY, camera, myScene->getSystem<CameraControllerSystem>()
			);

		auto renderSystem = myScene->getSystem<engine::RenderSystem>();
		renderSystem->setCameraEntity(camera);
	}

	/* shared resources */
	auto grassTexture = std::make_shared<engine::resources::Texture>(
		app.gfx(),
		app.renderData.materialSetLayout,
		app.renderData.materialSetSampler,
		app.getResourcePath("textures/grass.jpg"),
		engine::resources::Texture::Filtering::ANISOTROPIC,
		true,
		true
	);
	auto spaceTexture = std::make_shared<engine::resources::Texture>(
		app.gfx(),
		app.renderData.materialSetLayout,
		app.renderData.materialSetSampler,
		app.getResourcePath("textures/space2.png"),
		engine::resources::Texture::Filtering::ANISOTROPIC,
		true,
		true
	);

	/* cube */
	{
		uint32_t cube = myScene->createEntity("cube");
		myScene->getComponent<engine::TransformComponent>(cube)->position = glm::vec3{ -0.5f + 5.0f, -0.5f + 5.0f, -0.5f + 5.0f };
		auto cubeRenderable = myScene->addComponent<engine::RenderableComponent>(cube);
		cubeRenderable->material = std::make_shared<engine::resources::Material>(app.getResource<engine::resources::Shader>("builtin.standard"));
		cubeRenderable->material->m_texture = app.getResource<engine::resources::Texture>("builtin.white");
		cubeRenderable->mesh = genCuboidMesh(app.gfx(), 1.0f, 1.0f, 1.0f, 1);
		auto cubeCollider = myScene->addComponent<engine::ColliderComponent>(cube);
		cubeCollider->isStatic = true;
		cubeCollider->aabb = { { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f } };
	}

	/* floor */
	{
		uint32_t floor = myScene->createEntity("floor");
		myScene->getComponent<engine::TransformComponent>(floor)->position = glm::vec3{-5000.0f, -1.0f, -5000.0f};
		auto floorRenderable = myScene->addComponent<engine::RenderableComponent>(floor);
		floorRenderable->material = std::make_shared<engine::resources::Material>(app.getResource<engine::resources::Shader>("builtin.standard"));
		floorRenderable->material->m_texture = grassTexture;
		floorRenderable->mesh = genCuboidMesh(app.gfx(), 10000.0f, 1.0f, 10000.0f, 5000.0f);
		floorRenderable->shown = true;
		auto floorCollider = myScene->addComponent<engine::ColliderComponent>(floor);
		floorCollider->isStatic = true;
		floorCollider->aabb = { { 0.0f, 0.0f, 0.0f }, { 10000.0f, 1.0f, 10000.0f } };
	}

	//engine::util::loadMeshFromFile(myScene, app.getResourcePath("models/astronaut/astronaut.dae"));

	/* skybox */
	{
		uint32_t skybox = myScene->createEntity("skybox");
		auto skyboxRenderable = myScene->addComponent<engine::RenderableComponent>(skybox);
		skyboxRenderable->material = std::make_unique<engine::resources::Material>(app.getResource<engine::resources::Shader>("builtin.skybox"));
		skyboxRenderable->material->m_texture = spaceTexture;
		skyboxRenderable->mesh = genCuboidMesh(app.gfx(), 10.0f, 10.0f, 10.0f, 1.0f, true);
		myScene->getComponent<engine::TransformComponent>(skybox)->position = { -5.0f, -5.0f, -5.0f };
	}

	app.gameLoop();

}
