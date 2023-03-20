#pragma once

#include "resource_manager.hpp"

#include "gfx.hpp"
#include "gfx_device.hpp"

#include <glm/mat4x4.hpp>

#include <memory>
#include <string>
#include <filesystem>
#include <unordered_map>
#include <assert.h>

namespace engine {

	class Window; // "window.hpp"
	class GFXDevice; // "gfx_device.hpp"
	class InputManager; // "input_manager.hpp"
	class SceneManager; // "scene_manager.hpp"
	
	namespace resources {
		class Shader;
		class Texture;
	}

	struct RenderData {
		std::unique_ptr<GFXDevice> gfxdev;
		gfx::DrawBuffer* drawBuffer = nullptr;
		/* uniforms for engine globals */
		const gfx::DescriptorSetLayout* setZeroLayout;
		const gfx::DescriptorSet* setZero;
		struct SetZeroBuffer {
			glm::mat4 proj;
		};
		gfx::DescriptorBuffer* setZeroBuffer;
		/* uniforms for per-frame data */
		const gfx::DescriptorSetLayout* setOneLayout;
		const gfx::DescriptorSet* setOne;
		struct SetOneBuffer {
			glm::mat4 view;
		};
		gfx::DescriptorBuffer* setOneBuffer;
	};

	class Application {

	public:
		Application(const char* appName, const char* appVersion, gfx::GraphicsSettings graphicsSettings);
		~Application();
		Application(const Application&) = delete;
		Application& operator=(const Application&) = delete;

		/* resource stuff */

		template <typename T>
		void registerResourceManager()
		{
			size_t hash = typeid(T).hash_code();
			assert(m_resourceManagers.contains(hash) == false && "Registering resource manager type more than once.");
			m_resourceManagers.emplace(hash, std::make_unique<ResourceManager<T>>());
		}

		template <typename T>
		std::shared_ptr<T> addResource(const std::string& name, std::unique_ptr<T>&& resource)
		{
			auto resourceManager = getResourceManager<T>();
			return resourceManager->add(name, std::move(resource));
		}

		template <typename T>
		std::shared_ptr<T> getResource(const std::string& name)
		{
			auto resourceManager = getResourceManager<T>();
			return resourceManager->get(name);
		}

		/* methods */
		void gameLoop();

		void setFrameLimiter(bool on) { m_enableFrameLimiter = on; }

		/* getters */
		Window* window() { return m_window.get(); }
		GFXDevice* gfx() { return renderData.gfxdev.get(); }
		InputManager* inputManager() { return m_inputManager.get(); }
		SceneManager* sceneManager() { return m_sceneManager.get(); }

		std::string getResourcePath(const std::string relativePath) { return (m_resourcesPath / relativePath).string(); }

		RenderData renderData{};

	private:
		std::unique_ptr<Window> m_window;
		std::unique_ptr<InputManager> m_inputManager;
		std::unique_ptr<SceneManager> m_sceneManager;

		std::filesystem::path m_resourcesPath;

		bool m_enableFrameLimiter = true;

		/* resource stuff */

		std::unordered_map<size_t, std::unique_ptr<IResourceManager>> m_resourceManagers{};

		template <typename T>
		ResourceManager<T>* getResourceManager()
		{
			size_t hash = typeid(T).hash_code();
			auto it = m_resourceManagers.find(hash);
			if (it == m_resourceManagers.end()) {
				throw std::runtime_error("Cannot find resource manager.");
			}
			auto ptr = it->second.get();
			auto castedPtr = dynamic_cast<ResourceManager<T>*>(ptr);
			assert(castedPtr != nullptr);
			return castedPtr;
		}

	};

}
