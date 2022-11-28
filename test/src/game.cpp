#include "config.h"

#include "engine.hpp"

#include "window.hpp"
#include "input.hpp"
#include "sceneroot.hpp"

#include "components/camera.hpp"
#include "components/mesh_renderer.hpp"
#include "components/text_ui_renderer.hpp"

#include "resource_manager.hpp"
#include "resources/texture.hpp"
#include "resources/font.hpp"

#include "util/model_loader.hpp"

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
	camCamera->usePerspective(130.0f);
	cam->createComponent<CameraController>();

	/*
	auto gun = cam->createChild("gun");
	gun->transform.position = glm::vec3{ 0.2f, -0.1f, -0.15f };
	gun->transform.rotation = glm::angleAxis(glm::pi<float>(), glm::vec3{ 0.0f, 1.0f, 0.0f });
	float GUN_SCALE = 9.0f / 560.0f;
	gun->transform.scale *= GUN_SCALE;
	auto gunRenderer = gun->createComponent<engine::components::Renderer>();
	gunRenderer->setMesh("meshes/gun.mesh");
	gunRenderer->setTexture("textures/gun.png");
	*/

	// FLOOR
	constexpr float GRASS_DENSITY = 128.0f * 20.0f;
	auto floor = app.scene()->createChild("floor");
	auto floorRenderer = floor->createComponent<engine::components::Renderer>();
	floor->transform.position = glm::vec3{ 0.0f, 0.0f, 0.0f };
	floorRenderer->setTexture("textures/grass.jpg");
	floorRenderer->m_mesh = std::make_unique<engine::resources::Mesh>(std::vector<Vertex>{
		{ { -16.0f, 0.0f,  16.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f,			GRASS_DENSITY	} },
		{ {  16.0f, 0.0f, -16.0f }, { 0.0f, 1.0f, 0.0f }, { GRASS_DENSITY,	0.0f			} },
		{ { -16.0f, 0.0f, -16.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f,			0.0f			} },
		{ {  16.0f, 0.0f,  16.0f }, { 0.0f, 1.0f, 0.0f }, { GRASS_DENSITY,	GRASS_DENSITY	} },
		{ {  16.0f, 0.0f, -16.0f }, { 0.0f, 1.0f, 0.0f }, { GRASS_DENSITY,	0.0f			} },
		{ { -16.0f, 0.0f,  16.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f,			GRASS_DENSITY	} }
	});
	floor->transform.scale = { 100.0f, 1.0f, 100.0f };

	auto cube = app.scene()->createChild("cube");
	auto cubeRen = cube->createComponent<engine::components::Renderer>();
	cubeRen->setMesh("meshes/cube.mesh");
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

			constexpr float halfPitch = -glm::half_pi<float>() / 2.0f;
			glm::quat pitchQuat{};
			pitchQuat.x = glm::sin(halfPitch);
			pitchQuat.y = 0.0f;
			pitchQuat.z = 0.0f;
			pitchQuat.w = glm::cos(halfPitch);
			parent.transform.rotation *= pitchQuat;
		}
	private:
		float m_yaw = 0.0f;
	};
	cube->createComponent<Spin>();

	// boundary
	auto bounds = app.scene()->createChild("bounds");
	auto boundsRen = bounds->createComponent<engine::components::Renderer>();
	boundsRen->m_mesh = genSphereMesh(100.0f, 36, true);
	boundsRen->setTexture("textures/metal.jpg");

	auto message = app.scene()->createChild("message");
	message->transform.position = { -1.0f, 0.95f, 0.0f };
	message->transform.scale *= 0.5f;
	auto messageUI = message->createComponent<engine::components::UI>();
	class FPSTextUpdater : public engine::components::CustomComponent {
		engine::components::UI* textUI = nullptr;
	public:
		FPSTextUpdater(engine::Object* parent) : CustomComponent(parent)
		{
			textUI = parent->getComponent<engine::components::UI>();
		}
		void onUpdate(glm::mat4 t) override
		{
			textUI->m_text = std::to_string(parent.win.dt() * 1000.0f) + " ms";
		}
	};
	message->createComponent<FPSTextUpdater>();

	/*
	auto myModel = engine::util::loadAssimpMeshFromFile(app.scene(), app.resources()->getFilePath("models/pyramid/pyramid.dae").string());
	myModel->transform.position = { -5.0f, 2.0f, 7.0f };

	auto myRoom = engine::util::loadAssimpMeshFromFile(app.scene(), app.resources()->getFilePath("models/room/room.dae").string());
	myRoom->transform.position = { 9.0f, 0.1f, 3.0f };
	*/
	auto astronaut = engine::util::loadAssimpMeshFromFile(app.scene(), app.resources()->getFilePath("models/astronaut/astronaut.dae").string());
	astronaut->transform.position.z += 5.0f;
	astronaut->createComponent<Spin>();

/*
	auto lego = engine::util::loadAssimpMeshFromFile(app.scene(), app.resources()->getFilePath("models/lego/lego.dae").string());
	lego->transform.position = { -30.0f, -33.0f, -30.0f };
*/

	app.scene()->printTree();

	app.gameLoop();
}
