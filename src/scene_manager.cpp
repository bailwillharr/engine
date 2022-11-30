#include "scene_manager.hpp"

namespace engine {

	class Scene {};

	SceneManager::SceneManager()
	{
		m_scenes.emplace_back(std::make_unique<Scene>());
		m_activeScene = m_scenes.begin();
	}

	SceneManager::~SceneManager() {}

}