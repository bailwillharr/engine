#include "scene.hpp"

#include "ecs/transform.hpp"
#include "ecs/mesh_renderer.hpp"

namespace engine {

	Scene::Scene(Application* app)
		: m_app(app)
	{
		m_renderSystem = std::make_unique<ecs::RendererSystem>(this);
		m_transformSystem = std::make_unique<ecs::TransformSystem>(this);
	}

	Scene::~Scene() {}

	void Scene::update(float ts)
	{
		auto transforms = m_transformSystem->getMatrices();

		m_renderSystem->drawMeshes(*transforms);
	}

}
