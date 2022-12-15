#pragma once

#include "resource_manager.hpp"

#include <vector>
#include <memory>

namespace engine {

	class Scene; // "scene.hpp"
	namespace resources {
		class Texture;
	}

	class SceneManager {

	public:
		SceneManager();
		~SceneManager();
		SceneManager(const SceneManager&) = delete;
		SceneManager& operator=(const SceneManager&) = delete;

		Scene* createScene(std::unique_ptr<Scene>&& scene);

		void updateActiveScene(float ts);

		/* getters */
		ResourceManager<resources::Texture>* getTextureManager() { return m_textureManager.get(); }

	private:
		std::vector<std::unique_ptr<Scene>> m_scenes;
		int m_activeSceneIndex = -1;

		std::unique_ptr<ResourceManager<resources::Texture>> m_textureManager;

	};

}
