#include "engine.hpp"

#include "log.hpp"

#include "window.hpp"
#include "input.hpp"
#include "resource_manager.hpp"
#include "sceneroot.hpp"

#include "gfx_device.hpp"

#include "resources/mesh.hpp"

#include "components/mesh_renderer.hpp"
#include "components/camera.hpp"

namespace engine {

	Application::Application(const char* appName, const char* appVersion)
	{
		m_win = new Window(appName, true);

		gfxdev = new GFXDevice(appName, appVersion, m_win->getHandle());

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

		uint64_t lastTick = m_win->getNanos();
		constexpr int TICKFREQ = 1; // in hz

		// single-threaded game loop
		while (m_win->isRunning()) {

			/* logic */

			if (m_win->getLastFrameStamp() >= lastTick + (BILLION / TICKFREQ)) {
				lastTick = m_win->getLastFrameStamp();

				// do tick stuff here
				m_win->setTitle("frame time: " + std::to_string(m_win->dt() * 1000.0f) + " ms, " + std::to_string(m_win->getAvgFPS()) + " fps");
				m_win->resetAvgFPS();
			}

			if (m_win->getKeyPress(inputs::Key::F11)) {
				m_win->toggleFullscreen();
			}
			if (m_win->getKeyPress(inputs::Key::ESCAPE)) {
				m_win->setCloseFlag();
			}

			m_scene->updateStuff();

			/* draw */
			gfxdev->renderFrame();

			/* poll events */
			m_win->getInputAndEvents();

		}

		gfxdev->waitIdle();
	}

}
