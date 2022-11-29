#pragma once

#include <memory>

namespace engine {

	class Window;
	class GFXDevice;
	class InputManager;
	class SceneManager;

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

	private:
		std::unique_ptr<Window> m_window;
		std::unique_ptr<GFXDevice> m_gfx;
		std::unique_ptr<InputManager> m_inputManager;
		std::unique_ptr<SceneManager> m_sceneManager;

	};

}