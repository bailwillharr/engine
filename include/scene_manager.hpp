#pragma once

#include <vector>
#include <memory>

namespace engine {

	class Scene; // "scene.hpp"

	class SceneManager {

	public:
		SceneManager();
		~SceneManager();
		SceneManager(const SceneManager&) = delete;
		SceneManager& operator=(const SceneManager&) = delete;

		void createScene(std::unique_ptr<Scene>&& scene);

		void updateActiveScene();

	private:
		std::vector<std::unique_ptr<Scene>> m_scenes;
		int m_activeSceneIndex = -1;

	};

}