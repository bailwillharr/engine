#pragma once

#include <memory>

namespace engine {

	class Window;
	class GFXDevice;

	class Application {

	public:
		Application(const char* appName, const char* appVersion);

		Application(const Application&) = delete;
		Application& operator=(const Application&) = delete;

		~Application();

		void gameLoop();

	private:
		std::unique_ptr<Window> m_win;
		std::unique_ptr<GFXDevice> m_gfx;
	};

}
