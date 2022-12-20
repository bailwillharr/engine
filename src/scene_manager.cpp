#include "scene_manager.hpp"

#include "scene.hpp"
#include "resources/texture.hpp"

#include <assert.h>

namespace engine {

	SceneManager::SceneManager(Application* app)
		: m_app(app)
	{
		m_textureManager = std::make_unique<ResourceManager<resources::Texture>>();
	}

	SceneManager::~SceneManager() {}

	Scene* SceneManager::createScene()
	{
		auto scene = std::make_unique<Scene>(m_app);
		m_scenes.emplace_back(std::move(scene));
		m_activeSceneIndex = m_scenes.size() - 1;
		return m_scenes.back().get();
	}

	void SceneManager::updateActiveScene(float ts)
	{
		if (m_activeSceneIndex >= 0) {
			assert(m_activeSceneIndex < m_scenes.size());
			m_scenes[m_activeSceneIndex]->update(ts);
		}
	}

}
