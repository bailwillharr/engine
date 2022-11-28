#include "engine.hpp"

#include "log.hpp"

#include "window.hpp"
#include "gfx_device.hpp"
#include "input.hpp"

// To allow the FPS-limiter to put the thread to sleep
#include <thread>

namespace engine {

	Application::Application(const char* appName, const char* appVersion)
	{
		m_win = new Window(appName, true, true);
		m_gfx = new GFXDevice(appName, appVersion, m_win->getHandle());
		m_input = new Input(*m_win);
	}

	Application::~Application()
	{
		delete m_input;
		delete m_gfx;
		delete m_win;
	}

	void Application::gameLoop()
	{
		TRACE("Begin game loop...");

		constexpr int FPS_LIMIT = 240;
		constexpr auto FRAMETIME_LIMIT = std::chrono::nanoseconds(1000000000 / FPS_LIMIT);
		auto beginFrame = std::chrono::steady_clock::now();
		auto endFrame = beginFrame + FRAMETIME_LIMIT;

		// single-threaded game loop
		while (m_win->isRunning()) {

			/* logic */

			/* draw */
			m_gfx->renderFrame();

			/* poll events */
			m_win->getInputAndEvents();

			/* fps limiter */
			std::this_thread::sleep_until(endFrame);
			beginFrame = endFrame;
			endFrame = beginFrame + FRAMETIME_LIMIT;

		}

		m_gfx->waitIdle();

	}

}
