#include "scene_manager.hpp"

#include "scene.hpp"
#include "log.hpp"

#include <assert.h>

namespace engine {

	SceneManager::SceneManager()
	{
	}

	SceneManager::~SceneManager() {}

	Scene* SceneManager::createScene(std::unique_ptr<Scene>&& scene)
	{
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
