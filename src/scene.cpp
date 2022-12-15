#include "scene.hpp"

#include "components/mesh_renderer.hpp"

namespace engine {

	Scene::Scene()
	{
		m_renderSystem = std::make_unique<RendererSystem>();
	}

	Scene::~Scene() {}

	void Scene::update(float ts)
	{
		m_renderSystem->onUpdate(ts);
	}

}
