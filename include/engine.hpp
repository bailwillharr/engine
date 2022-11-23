#pragma once

#include <memory>

namespace engine {

	class Window;
	class Input;
	class ResourceManager;
	class SceneRoot;

	class Application {

	public:
		Application(const char* appName, const char* appVersion);

		Application(const Application&) = delete;
		Application& operator=(const Application&) = delete;

		~Application();

		void gameLoop();

		Window* window()
		{
			return m_win;
		}

		Input* input()
		{
			return m_input;
		}

		ResourceManager* resources()
		{
			return m_res;
		}

		SceneRoot* scene()
		{
			return m_scene;
		}



	private:
		Window* m_win;
		Input* m_input;
		ResourceManager* m_res;
		SceneRoot* m_scene;

		bool m_enableFrameLimiter = true;
	};

}
