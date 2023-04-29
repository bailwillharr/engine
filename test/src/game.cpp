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
	inputManager->AddInputButton("fullscreen", engine::inputs::Key::K_F11);
	inputManager->AddInputButton("exit", engine::inputs::Key::K_ESCAPE);
	// game buttons
	inputManager->AddInputButton("fire", engine::inputs::MouseButton::M_LEFT);
	inputManager->AddInputButton("aim", engine::inputs::MouseButton::M_RIGHT);
	inputManager->AddInputButton("jump", engine::inputs::Key::K_SPACE);
	inputManager->AddInputButton("sprint", engine::inputs::Key::K_LSHIFT);
	// game movement
	inputManager->AddInputButtonAsAxis("movex", engine::inputs::Key::K_D, engine::inputs::Key::K_A);
	inputManager->AddInputButtonAsAxis("movey", engine::inputs::Key::K_W, engine::inputs::Key::K_S);
	// looking around
	inputManager->AddInputAxis("lookx", engine::inputs::MouseAxis::X);
	inputManager->AddInputAxis("looky", engine::inputs::MouseAxis::Y);
}

void playGame(GameSettings settings)
{
	LOG_INFO("FPS limiter: {}", settings.enableFrameLimiter ? "ON" : "OFF");
	LOG_INFO("Graphics Validation: {}", settings.enableValidation ? "ON" : "OFF");

	engine::gfx::GraphicsSettings graphicsSettings{};
	graphicsSettings.enable_validation = settings.enableValidation;
	graphicsSettings.vsync = true;
	graphicsSettings.wait_for_present = false;
	graphicsSettings.msaa_level = engine::gfx::MSAALevel::kOff;
	engine::Application app(PROJECT_NAME, PROJECT_VERSION, graphicsSettings);

	app.SetFrameLimiter(settings.enableFrameLimiter);

	// configure window
	app.window()->SetRelativeMouseMode(true);
	
	configureInputs(app.input_manager());

	auto myScene = app.scene_manager()->CreateEmptyScene();

	/* create camera */
	{
		myScene->registerComponent<CameraControllerComponent>();
		myScene->RegisterSystem<CameraControllerSystem>();

		auto camera = myScene->CreateEntity("camera");
		myScene->GetComponent<engine::TransformComponent>(camera)->position = { 0.0f, 10.0f, 0.0f };
		auto cameraCollider = myScene->AddComponent<engine::ColliderComponent>(camera);
		cameraCollider->isStatic = false;
		cameraCollider->isTrigger = true;
		cameraCollider->aabb = { { -0.2f, -1.5f, -0.2f }, { 0.2f, 0.2f, 0.2f} }; // Origin is at eye level
		myScene->AddComponent<CameraControllerComponent>(camera);
		myScene->event_system()->SubscribeToEventType<engine::PhysicsSystem::CollisionEvent>(
				engine::EventSubscriberKind::kEntity, camera, myScene->GetSystem<CameraControllerSystem>()
			);

		auto renderSystem = myScene->GetSystem<engine::RenderSystem>();
		renderSystem->setCameraEntity(camera);
	}

	/* shared resources */
	auto grassTexture = std::make_shared<engine::resources::Texture>(
		&app.render_data_,
		app.GetResourcePath("textures/grass.jpg"),
		engine::resources::Texture::Filtering::ANISOTROPIC
	);
	auto spaceTexture = std::make_shared<engine::resources::Texture>(
		&app.render_data_,
		app.GetResourcePath("textures/space2.png"),
		engine::resources::Texture::Filtering::ANISOTROPIC
	);

	/* cube */
	{
		uint32_t cube = myScene->CreateEntity("cube");
		myScene->GetComponent<engine::TransformComponent>(cube)->position = glm::vec3{ -0.5f + 5.0f, -0.5f + 5.0f, -0.5f + 5.0f };
		auto cubeRenderable = myScene->AddComponent<engine::RenderableComponent>(cube);
		cubeRenderable->material = std::make_shared<engine::resources::Material>(app.GetResource<engine::resources::Shader>("builtin.standard"));
		cubeRenderable->material->m_texture = app.GetResource<engine::resources::Texture>("builtin.white");
		cubeRenderable->mesh = genCuboidMesh(app.gfxdev(), 1.0f, 1.0f, 1.0f, 1);
		auto cubeCollider = myScene->AddComponent<engine::ColliderComponent>(cube);
		cubeCollider->isStatic = true;
		cubeCollider->aabb = { { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f } };
	}

	/* floor */
	{
		uint32_t floor = myScene->CreateEntity("floor");
		myScene->GetComponent<engine::TransformComponent>(floor)->position = glm::vec3{-5000.0f, -1.0f, -5000.0f};
		auto floorRenderable = myScene->AddComponent<engine::RenderableComponent>(floor);
		floorRenderable->material = std::make_shared<engine::resources::Material>(app.GetResource<engine::resources::Shader>("builtin.standard"));
		floorRenderable->material->m_texture = grassTexture;
		floorRenderable->mesh = genCuboidMesh(app.gfxdev(), 10000.0f, 1.0f, 10000.0f, 5000.0f);
		floorRenderable->shown = true;
		auto floorCollider = myScene->AddComponent<engine::ColliderComponent>(floor);
		floorCollider->isStatic = true;
		floorCollider->aabb = { { 0.0f, 0.0f, 0.0f }, { 10000.0f, 1.0f, 10000.0f } };
	}

	//engine::util::loadMeshFromFile(myScene, app.GetResourcePath("models/astronaut/astronaut.dae"));

	/* skybox */
	{
		uint32_t skybox = myScene->CreateEntity("skybox");
		auto skyboxRenderable = myScene->AddComponent<engine::RenderableComponent>(skybox);
		skyboxRenderable->material = std::make_unique<engine::resources::Material>(app.GetResource<engine::resources::Shader>("builtin.skybox"));
		skyboxRenderable->material->m_texture = spaceTexture;
		skyboxRenderable->mesh = genCuboidMesh(app.gfxdev(), 10.0f, 10.0f, 10.0f, 1.0f, true);
		myScene->GetComponent<engine::TransformComponent>(skybox)->position = { -5.0f, -5.0f, -5.0f };
	}

	app.GameLoop();

}
