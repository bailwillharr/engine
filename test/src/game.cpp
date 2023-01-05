#include "config.h"

#include "camera_controller.hpp"

#include "application.hpp"
#include "window.hpp"
#include "input_manager.hpp"
#include "scene_manager.hpp"
#include "scene.hpp"

#include "components/transform.hpp"
#include "components/renderable.hpp"

#include "systems/transform.hpp"
#include "systems/render.hpp"

#include "resources/mesh.hpp"
#include "resources/material.hpp"
#include "resources/shader.hpp"
#include "resources/texture.hpp"

#include "util/model_loader.hpp"

struct RotateComponent {
	float rotation = 0.0f;
};

class RotateSystem : public engine::System {
public:
		RotateSystem(engine::Scene* scene)
			: System(scene, { typeid(engine::TransformComponent).hash_code(), typeid(RotateComponent).hash_code() })
		{
		}

		void onUpdate(float ts) override
		{
			for (uint32_t entity : m_entities) {
				auto t = m_scene->getComponent<engine::TransformComponent>(entity);
				auto r = m_scene->getComponent<RotateComponent>(entity);
				r->rotation += ts;
				r->rotation = glm::mod(r->rotation, glm::two_pi<float>());
				t->rotation = glm::angleAxis(r->rotation, glm::vec3{0.0f, 0.0f, 1.0f});
			}
		}
};



void playGame()
{
	engine::Application app(PROJECT_NAME, PROJECT_VERSION);

	app.setFrameLimiter(false);

	// configure window
	app.window()->setRelativeMouseMode(true);



	// input config

	// user interface mappings
	app.inputManager()->addInputButton("fullscreen", engine::inputs::Key::K_F11);
	// game buttons
	app.inputManager()->addInputButton("fire", engine::inputs::MouseButton::M_LEFT);
	app.inputManager()->addInputButton("aim", engine::inputs::MouseButton::M_RIGHT);
	app.inputManager()->addInputButton("jump", engine::inputs::Key::K_SPACE);
	app.inputManager()->addInputButton("sprint", engine::inputs::Key::K_LSHIFT);
	// game movement
	app.inputManager()->addInputButtonAsAxis("movex", engine::inputs::Key::K_D, engine::inputs::Key::K_A);
	app.inputManager()->addInputButtonAsAxis("movey", engine::inputs::Key::K_W, engine::inputs::Key::K_S);
	// looking around
	app.inputManager()->addInputAxis("lookx", engine::inputs::MouseAxis::X);
	app.inputManager()->addInputAxis("looky", engine::inputs::MouseAxis::Y);



	auto myScene = app.sceneManager()->createEmptyScene();

	myScene->registerComponent<RotateComponent>();
	myScene->registerComponent<CameraControllerComponent>();

	myScene->registerSystem<RotateSystem>();
	myScene->registerSystem<CameraControllerSystem>();

	myScene->registerResourceManager<engine::resources::Shader>();
	myScene->registerResourceManager<engine::resources::Texture>();

	auto camera = myScene->createEntity("camera");
	myScene->addComponent<CameraControllerComponent>(camera);
	myScene->getSystem<engine::RenderSystem>()->setCameraEntity(camera);

	engine::resources::Shader::VertexParams vertParams{};
	vertParams.hasNormal = true;
	vertParams.hasUV0 = true;
	auto theShader = std::make_unique<engine::resources::Shader>(
		app.gfx(),
		app.getResourcePath("engine/shaders/texture.vert").c_str(),
		app.getResourcePath("engine/shaders/texture.frag").c_str(),
		vertParams,
		false,
		true
		);
	auto whiteTexture = std::make_unique<engine::resources::Texture>(
		app.gfx(),
		app.getResourcePath("engine/textures/white.png")
	);
	auto keepTexture = myScene->addResource<engine::resources::Texture>("whiteTexture", std::move(whiteTexture));
	auto keepShader = myScene->addResource<engine::resources::Shader>("theShader", std::move(theShader));

	engine::util::loadMeshFromFile(myScene, app.getResourcePath("models/lego/lego.dae"));

	app.gameLoop();

	INFO("texture addr: {}, shader addr: {}", (void*)keepTexture->getHandle(), (void*)keepShader->getPipeline());
}
