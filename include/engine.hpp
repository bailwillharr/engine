#pragma once

#include <memory>

namespace engine {

	class Window;
	class Input;
	class GFXDevice;
	class ResourceManager;
	class SceneRoot;

	class Application {

	public:
		Application(const char* appName, const char* appVersion);

		Application(const Application&) = delete;
		Application& operator=(const Application&) = delete;

		~Application();

		void gameLoop();

	private:
		std::unique_ptr<Window> m_win;
		std::unique_ptr<Input> m_input;
		std::unique_ptr<ResourceManager> m_res;
		std::unique_ptr<SceneRoot> m_scene;
	};

}
