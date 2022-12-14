#include "scene_manager.hpp"

#include "scene.hpp"
#include "texture_manager.hpp"
#include "log.hpp"

namespace engine {

	SceneManager::SceneManager()
		: m_textureManager(std::make_unique<TextureManager>())
	{
		auto tex = std::make_unique<Texture>();
		m_textureManager->add("myTexture", std::move(tex));
	}

	SceneManager::~SceneManager() {}

	void SceneManager::createScene(std::unique_ptr<Scene>&& scene)
	{
		m_scenes.emplace_back(std::move(scene));
	}

	void SceneManager::updateActiveScene()
	{
		if (m_activeSceneIndex >= 0) {
		}
	}

}
