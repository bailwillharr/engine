#pragma once

namespace engine {

	class Window;
	class GFXDevice;
	class Input;

	class Application {
	public:
		Application(const char* appName, const char* appVersion);

		Application(const Application&) = delete;
		Application& operator=(const Application&) = delete;

		~Application();

		void gameLoop();

		Window* window() { return m_win; }
		GFXDevice* gfx() { return m_gfx; }
		Input* input() { return m_input; }

	private:
		Window* m_win;
		GFXDevice* m_gfx;
		Input* m_input;

	};

}
