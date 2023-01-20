#pragma once

#include "resource_manager.hpp"

#include <vector>
#include <memory>

namespace engine {

	class Application;
	class Scene; // "scene.hpp"

	class SceneManager {

	public:
		SceneManager(Application* app);
		~SceneManager();
		SceneManager(const SceneManager&) = delete;
		SceneManager& operator=(const SceneManager&) = delete;

		// creates an empty scene and sets it as active
		Scene* createEmptyScene();

		void updateActiveScene(float ts);

	private:
		Application* const m_app;

		std::vector<std::unique_ptr<Scene>> m_scenes;
		int m_activeSceneIndex = -1;

	};

}
