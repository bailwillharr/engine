#pragma once

#include "resource_manager.hpp"

#include <vector>
#include <memory>

namespace engine {

	class Application;
	class Scene; // "scene.hpp"
	namespace resources {
		class Texture;
	}

	class SceneManager {

	public:
		SceneManager(Application* app);
		~SceneManager();
		SceneManager(const SceneManager&) = delete;
		SceneManager& operator=(const SceneManager&) = delete;

		Scene* createScene();

		void updateActiveScene(float ts);

		/* getters */
		ResourceManager<resources::Texture>* getTextureManager() { return m_textureManager.get(); }

	private:
		Application* const m_app;

		std::vector<std::unique_ptr<Scene>> m_scenes;
		int m_activeSceneIndex = -1;

		std::unique_ptr<ResourceManager<resources::Texture>> m_textureManager;

	};

}
