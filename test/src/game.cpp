#include "config.h"

#include "application.hpp"
#include "window.hpp"
#include "scene_manager.hpp"
#include "scene.hpp"
#include "resources/material.hpp"
#include "resources/shader.hpp"
#include "resources/texture.hpp"
#include "resources/mesh.hpp"
#include "ecs/transform.hpp"
#include "ecs/mesh_renderer.hpp"

void playGame()
{
	engine::Application app(PROJECT_NAME, PROJECT_VERSION);

	// configure window
	app.window()->setRelativeMouseMode(false);

	auto myScene = app.sceneManager()->createScene();

	myScene->m_renderSystem->m_cameraPos = { 0.0f, 2.0f, 0.0f };
	myScene->m_renderSystem->m_cameraRot = glm::angleAxis(0.0f, glm::vec3{ 0.0f, 0.0f, -1.0f });

	{
		auto entity1 = myScene->createEntity();
		engine::gfx::VertexFormat vertFormat{};
		vertFormat.stride = sizeof(float) * 8;
		vertFormat.attributeDescriptions.emplace_back(0, engine::gfx::VertexAttribFormat::VEC3, 0);
		vertFormat.attributeDescriptions.emplace_back(1, engine::gfx::VertexAttribFormat::VEC3, sizeof(float) * 3);
		vertFormat.attributeDescriptions.emplace_back(2, engine::gfx::VertexAttribFormat::VEC2, sizeof(float) * 6);
		auto myShader = std::make_unique<engine::resources::Shader>(
				app.gfx(),
				app.getResourcePath("engine/shaders/texture.vert").c_str(),
				app.getResourcePath("engine/shaders/texture.frag").c_str(),
				vertFormat,
				false,
				false
			);
		auto myMaterial = std::make_unique<engine::resources::Material>(std::move(myShader));
		std::vector<engine::Vertex> triVertices{
			{ {  0.5f,  0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f } },
			{ { -0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f } },
			{ {  0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f } },
		};
		myMaterial->m_texture = std::make_unique<engine::resources::Texture>(app.gfx(), app.getResourcePath("textures/grass.jpg"));
		auto myMesh = std::make_unique<engine::resources::Mesh>(app.gfx(), triVertices);
		myScene->m_renderSystem->m_components.emplace(
			entity1,
			engine::ecs::MeshRendererComponent{
				std::move(myMaterial),
				std::move(myMesh),
			}
		);
		myScene->m_transformSystem->m_components.emplace(
			entity1,
			engine::ecs::TransformComponent{
				.parent = 0,
				.scale = {1.0f, 1.0f, 1.0f},
				.position = {0.0f, 0.0f, -10.0f},
				.rotation = glm::angleAxis(glm::radians(24.0f), glm::vec3{ 0.0f, 1.0f, 0.0f })
			}
		);
	}

	app.gameLoop();
}
