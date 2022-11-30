#pragma once

#include <list>
#include <memory>

namespace engine {

	class Scene; // "scene.hpp"

	class SceneManager {

	public:
		SceneManager();
		~SceneManager();
		SceneManager(const SceneManager&) = delete;
		SceneManager& operator=(const SceneManager&) = delete;

	private:
		std::list<std::unique_ptr<Scene>> m_scenes;
		std::list<std::unique_ptr<Scene>>::iterator m_activeScene{};

	};

}