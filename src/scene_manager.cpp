#include "scene_manager.hpp"

#include "scene.hpp"

#include "log.hpp"

namespace engine {

	SceneManager::SceneManager()
	{
		
	}

	SceneManager::~SceneManager() {}

	void SceneManager::createScene(std::unique_ptr<Scene>&& scene)
	{
		m_scenes.emplace_back(std::move(scene));
	}

	void SceneManager::updateActiveScene()
	{
		if (m_activeSceneIndex >= 0) {
			INFO("updating scene: {}", m_scenes[m_activeSceneIndex]->name);
		}
	}

}