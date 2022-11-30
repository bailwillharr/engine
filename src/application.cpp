#include "application.hpp"

#include "log.hpp"

#include "window.hpp"
#include "gfx_device.hpp"
#include "input_manager.hpp"
#include "scene_manager.hpp"

#include "scene.hpp"

// To allow the FPS-limiter to put the thread to sleep
#include <thread>

namespace engine {

	Application::Application(const char* appName, const char* appVersion)
	{
		m_window = std::make_unique<Window>(appName, true, false);
		m_gfx = std::make_unique<GFXDevice>(appName, appVersion, m_window->getHandle());
		m_inputManager = std::make_unique<InputManager>(window());
		m_sceneManager = std::make_unique<SceneManager>();

		auto myScene = std::make_unique<Scene>();
		m_sceneManager->createScene(std::move(myScene));
	}

	Application::~Application() {}

	void Application::gameLoop()
	{
		TRACE("Begin game loop...");

		constexpr int FPS_LIMIT = 240;
		constexpr auto FRAMETIME_LIMIT = std::chrono::nanoseconds(1000000000 / FPS_LIMIT);
		auto beginFrame = std::chrono::steady_clock::now();
		auto endFrame = beginFrame + FRAMETIME_LIMIT;

		// single-threaded game loop
		while (m_window->isRunning()) {

			/* logic */
			m_sceneManager->updateActiveScene();

			/* draw */
			m_gfx->renderFrame();

			/* poll events */
			m_window->getInputAndEvents();

			/* fps limiter */
			std::this_thread::sleep_until(endFrame);
			beginFrame = endFrame;
			endFrame = beginFrame + FRAMETIME_LIMIT;

		}

		m_gfx->waitIdle();

	}

}
