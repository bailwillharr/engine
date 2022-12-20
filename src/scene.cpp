#include "scene.hpp"

#include "ecs/mesh_renderer.hpp"

namespace engine {

	Scene::Scene(Application* app)
		: m_app(app)
	{
		m_renderSystem = std::make_unique<ecs::RendererSystem>(this);
	}

	Scene::~Scene() {}

	void Scene::update(float ts)
	{
		m_renderSystem->onUpdate(ts);
	}

}
