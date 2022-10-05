#include "engine.hpp"

#include "window.hpp"
#include "gfx_device.hpp"

namespace engine {

	Application::Application(const char* appName, const char* appVersion)
	{
		m_win = std::make_unique<Window>(appName);
		m_gfx = std::make_unique<GFXDevice>(appName, appVersion, m_win->getHandle());
	}

	Application::~Application()
	{

	}

	void Application::gameLoop()
	{
		uint64_t lastTick = m_win->getNanos();
		constexpr int TICKFREQ = 20; // in hz

		// single-threaded game loop
		while (m_win->isRunning()) {

			/* logic */

			if (m_win->getLastFrameStamp() >= lastTick + (BILLION / TICKFREQ)) {
				lastTick = m_win->getLastFrameStamp();

				// do tick stuff here

			}

			if (m_win->getKeyPress(inputs::Key::F11)) {
				if (m_win->isFullscreen()) {
					m_win->setFullscreen(false);
				}
				else {
					m_win->setFullscreen(true, false); // borderless window
				}
			}
			if (m_win->getKeyPress(inputs::Key::ESCAPE)) {
				m_win->setCloseFlag();
			}

			/* draw */
			m_gfx->draw();

			/* poll events */
			m_win->getInputAndEvents();

		}
	}

}
