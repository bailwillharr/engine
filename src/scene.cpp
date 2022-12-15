#include "scene.hpp"

namespace engine {

	Scene::Scene()
	{
		m_renderSystem = std::make_unique<ecs::RendererSystem>();
	}

	Scene::~Scene() {}

	void Scene::update(float ts)
	{
		m_renderSystem->onUpdate(ts);
	}

}
