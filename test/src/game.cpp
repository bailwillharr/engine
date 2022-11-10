#include "config.h"

#include "engine.hpp"

#include "window.hpp"
#include "input.hpp"
#include "sceneroot.hpp"

#include "components/camera.hpp"
#include "components/mesh_renderer.hpp"

#include "resource_manager.hpp"
#include "resources/texture.hpp"

#include "camera_controller.hpp"
#include "meshgen.hpp"

#include <glm/gtc/quaternion.hpp>

#include <log.hpp>

#include <string>

void playGame()
{
	engine::Application app(PROJECT_NAME, PROJECT_VERSION);

	// configure window
	app.window()->setRelativeMouseMode(true);



	// input config
	
	// game buttons
	app.input()->addInputButton("fire", engine::inputs::MouseButton::M_LEFT);
	app.input()->addInputButton("aim", engine::inputs::MouseButton::M_RIGHT);
	app.input()->addInputButton("jump", engine::inputs::Key::SPACE);
	app.input()->addInputButton("sneak", engine::inputs::Key::LSHIFT);
	// game movement
	app.input()->addInputButtonAsAxis("movex", engine::inputs::Key::D, engine::inputs::Key::A);
	app.input()->addInputButtonAsAxis("movey", engine::inputs::Key::W, engine::inputs::Key::S);
	// looking around
	app.input()->addInputAxis("lookx", engine::inputs::MouseAxis::X);
	app.input()->addInputAxis("looky", engine::inputs::MouseAxis::Y);



	// create the scene

	auto cam = app.scene()->createChild("cam");
	constexpr float HEIGHT_INCHES = 6.0f * 12.0f;
	// eye level is about 4 1/2 inches below height
	constexpr float EYE_LEVEL = (HEIGHT_INCHES - 4.5f) * 25.4f / 1000.0f;
	cam->transform.position = { 0.0f, EYE_LEVEL, 0.0f };
	auto camCamera = cam->createComponent<engine::components::Camera>();
	camCamera->usePerspective(70.0f);
	cam->createComponent<CameraController>();
	cam->createComponent<engine::components::Renderer>()->m_mesh = genSphereMesh(0.2f, 20);
	cam->getComponent<engine::components::Renderer>()->setTexture("textures/cobble_stone.png");

	auto gun = cam->createChild("gun");
	gun->transform.position = glm::vec3{ 0.2f, -0.1f, -0.15f };
	gun->transform.rotation = glm::angleAxis(glm::pi<float>(), glm::vec3{ 0.0f, 1.0f, 0.0f });
	float GUN_SCALE = 9.0f / 560.0f;
	GUN_SCALE *= 1.0f;
	gun->transform.scale *= GUN_SCALE;
	auto gunRenderer = gun->createComponent<engine::components::Renderer>();
	gunRenderer->setMesh("meshes/gun.mesh");
	gunRenderer->setTexture("textures/gun.png");
	gunRenderer->m_color = { 0.2f, 0.3f, 0.2f };

	// FLOOR

	constexpr float GRASS_DENSITY = 128.0f * 20.0f;
	auto floor = app.scene()->createChild("floor");
	auto floorRenderer = floor->createComponent<engine::components::Renderer>();
	floor->transform.position = glm::vec3{ 0.0f, 0.0f, 0.0f };
	floorRenderer->setTexture("textures/stone_bricks.png");
	floorRenderer->m_mesh = std::make_unique<engine::resources::Mesh>(std::vector<Vertex>{
		{ { -16.0f, 0.0f,  16.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f,			GRASS_DENSITY	} },
		{ {  16.0f, 0.0f, -16.0f }, { 0.0f, 1.0f, 0.0f }, { GRASS_DENSITY,	0.0f			} },
		{ { -16.0f, 0.0f, -16.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f,			0.0f			} },
		{ {  16.0f, 0.0f,  16.0f }, { 0.0f, 1.0f, 0.0f }, { GRASS_DENSITY,	GRASS_DENSITY	} },
		{ {  16.0f, 0.0f, -16.0f }, { 0.0f, 1.0f, 0.0f }, { GRASS_DENSITY,	0.0f			} },
		{ { -16.0f, 0.0f,  16.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f,			GRASS_DENSITY	} }
	});
	floor->transform.scale = { 100.0f, 1.0f, 100.0f };
	floorRenderer->m_color = { 0.1f, 0.9f, 0.1f };

	auto cube = app.scene()->createChild("cube");
	auto cubeRen = cube->createComponent<engine::components::Renderer>();
	cubeRen->setMesh("meshes/cube.mesh");
	cubeRen->m_color = { 0.8f, 0.2f, 0.05f };

	cube->transform.position = glm::vec3{ -5.0f, 1.0f, 0.0f };
	class Spin : public engine::components::CustomComponent {

	public:
		Spin(engine::Object* parent) : CustomComponent(parent)
		{

		}
		void onUpdate(glm::mat4 t) override
		{
			m_yaw += win.dt();
			m_yaw = glm::mod(m_yaw, glm::two_pi<float>());
			const float halfYaw = m_yaw / 2.0f;
			glm::quat yawQuat{};
			yawQuat.x = 0.0f;
			yawQuat.y = glm::sin(halfYaw);
			yawQuat.z = 0.0f;
			yawQuat.w = glm::cos(halfYaw);
			parent.transform.rotation = yawQuat;
		}

	private:
		float m_yaw = 0.0f;

	};
	app.scene()->getChild("cube")->createComponent<Spin>();

	auto sphere = app.scene()->createChild("sphere");

	sphere->transform.position = { 10.0f, 5.0f, 10.0f };

	auto sphereRenderer = sphere->createComponent<engine::components::Renderer>();
	sphereRenderer->m_mesh = genSphereMesh(5.0f, 100, false);
	sphereRenderer->setTexture("textures/cobble_stone.png");

	app.gameLoop();
}