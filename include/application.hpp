#pragma once

#include <memory>
#include <string>
#include <filesystem>

namespace engine {

	class Window; // "window.hpp"
	class GFXDevice; // "gfx_device.hpp"
	class InputManager; // "input_manager.hpp"
	class SceneManager; // "scene_manager.hpp"

	class Application {

	public:
		Application(const char* appName, const char* appVersion);
		~Application();
		Application(const Application&) = delete;
		Application& operator=(const Application&) = delete;

		/* methods */
		void gameLoop();

		/* getters */
		Window* window() { return m_window.get(); }
		GFXDevice* gfx() { return m_gfx.get(); }
		InputManager* inputManager() { return m_inputManager.get(); }
		SceneManager* sceneManager() { return m_sceneManager.get(); }

		std::string getResourcePath(const std::string relativePath) { return (m_resourcesPath / relativePath).string(); }

	private:
		std::unique_ptr<Window> m_window;
		std::unique_ptr<GFXDevice> m_gfx;
		std::unique_ptr<InputManager> m_inputManager;
		std::unique_ptr<SceneManager> m_sceneManager;

		std::filesystem::path m_resourcesPath;

	};

}
