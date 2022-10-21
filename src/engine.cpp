#include "engine.hpp"

#include "window.hpp"
#include "gfx_device.hpp"
#include "resource_manager.hpp"

namespace engine {

	Application::Application(const char* appName, const char* appVersion)
	{
		m_win = std::make_unique<Window>(appName, true);
		m_gfx = std::make_unique<GFXDevice>(appName, appVersion, m_win->getHandle());

		engine::ResourceManager resMan{};

		m_gfx->createPipeline(resMan.getFilePath("shader.vert.spv").string().c_str(), resMan.getFilePath("shader.frag.spv").string().c_str());
	}

	Application::~Application()
	{

	}

	void Application::gameLoop()
	{
		uint64_t lastTick = m_win->getNanos();
		constexpr int TICKFREQ = 1; // in hz

		// single-threaded game loop
		while (m_win->isRunning()) {

			/* logic */

			if (m_win->getLastFrameStamp() >= lastTick + (BILLION / TICKFREQ)) {
				lastTick = m_win->getLastFrameStamp();

				// do tick stuff here
				m_win->setTitle("frame time: " + std::to_string(m_win->dt() * 1000.0f) + " ms, " + std::to_string(m_win->getFPS()) + " fps");

			}

			if (m_win->getKeyPress(inputs::Key::F11)) {
				m_win->toggleFullscreen();
			}
			if (m_win->getKeyPress(inputs::Key::ESCAPE)) {
				m_win->setCloseFlag();
			}

			/* draw */
			m_gfx->draw();

			/* poll events */
			m_win->getInputAndEvents();

		}

		m_gfx->waitIdle();
	}

}
