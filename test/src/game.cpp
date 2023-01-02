#include "config.h"

#include "application.hpp"
#include "window.hpp"
#include "scene_manager.hpp"
#include "scene.hpp"

#include "components/transform.hpp"
#include "components/renderable.hpp"

#include "systems/transform.hpp"

#include "resources/mesh.hpp"
#include "resources/material.hpp"
#include "resources/shader.hpp"
#include "resources/texture.hpp"

void playGame()
{
	engine::Application app(PROJECT_NAME, PROJECT_VERSION);

	// configure window
	app.window()->setRelativeMouseMode(true);

	auto myScene = app.sceneManager()->createEmptyScene();

	std::shared_ptr<engine::resources::Material> myMaterial;
	{
		engine::resources::Shader::VertexParams vertParams{};
		vertParams.hasNormal = true;
		vertParams.hasUV0 = true;
		auto myShader = std::make_unique<engine::resources::Shader>(
			app.gfx(),
			app.getResourcePath("engine/shaders/texture.vert").c_str(),
			app.getResourcePath("engine/shaders/texture.frag").c_str(),
			vertParams,
			false,
			true
		);
		myMaterial = std::make_shared<engine::resources::Material>(std::move(myShader));
		myMaterial->m_texture = std::make_shared<engine::resources::Texture>(
			app.gfx(),
			app.getResourcePath("engine/textures/white.png")
		);
	}

	auto myMesh = std::make_shared<engine::resources::Mesh>(
		app.gfx(),
		std::vector<engine::Vertex>{
			{ {  0.5f,  0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f } },
			{ { -0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f } },
			{ {  0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f } },
		}
	);

	constexpr int N = 100;

	for (int i = 0; i < N; i++) {
		for (int j = 0; j < N; j++) {

			auto entity = myScene->createEntity(std::to_string(i * 10 + j));

			auto t = myScene->getComponent<engine::TransformComponent>(entity);
			float x = ((float)(i) * 10.0f / (float)(N) ) - 5.0f;
			float y = ((float)(j) * 10.0f / (float)(N) ) - 5.0f;
			t->position = {x, y, -35.0f};
			t->scale /= (float)(N) / 4.0f;

			auto r = myScene->addComponent<engine::RenderableComponent>(entity);
			r->material = myMaterial;
			r->mesh = myMesh;

		}
	}

	app.gameLoop();
}
