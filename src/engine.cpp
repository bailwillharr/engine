#include "engine.hpp"

#include "log.hpp"

#include "window.hpp"
#include "input.hpp"
#include "resource_manager.hpp"
#include "sceneroot.hpp"

#include "gfx_device.hpp"

// To allow the FPS-limiter to put the thread to sleep
#include <thread>

namespace engine {

	Application::Application(const char* appName, const char* appVersion)
	{
		m_win = new Window(appName, true, true);

		gfxdev = new GFXDevice(appName, appVersion, m_win->getHandle(), false);

		m_input = new Input(*m_win);
		m_res = new ResourceManager();

		GameIO things{
			m_win,
			m_input,
			m_res
		};
		m_scene = new SceneRoot(things);
	}

	Application::~Application()
	{
		delete m_scene;
		delete m_res;
		delete m_input;

		delete gfxdev;

		delete m_win;
	}

	void Application::gameLoop()
	{
		TRACE("Begin game loop...");

		constexpr int FPS_LIMIT = 240;
		constexpr auto FRAMETIME_LIMIT = std::chrono::nanoseconds(1000000000 / FPS_LIMIT);
		auto beginFrame = std::chrono::steady_clock::now();
		auto endFrame = beginFrame + FRAMETIME_LIMIT;

		//m_enableFrameLimiter = false;

		// single-threaded game loop
		while (m_win->isRunning()) {

			m_scene->updateStuff();

			/* draw */
			gfxdev->renderFrame();

			/* poll events */
			m_win->getInputAndEvents();

			/* fps limiter */
			if (m_enableFrameLimiter) {
				std::this_thread::sleep_until(endFrame);
			}
			beginFrame = endFrame;
			endFrame = beginFrame + FRAMETIME_LIMIT;

		}

		gfxdev->waitIdle();

	}

}
