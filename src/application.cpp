#include "application.hpp"

#include "log.hpp"

#include "window.hpp"
#include "gfx_device.hpp"
#include "input_manager.hpp"
#include "scene_manager.hpp"

#include "scene.hpp"

#include "resources/mesh.hpp"
#include "resources/material.hpp"
#include "resources/shader.hpp"
#include "resources/texture.hpp"

// To allow the FPS-limiter to put the thread to sleep
#include <thread>

#ifdef _MSC_VER
#include <windows.h>
#include <direct.h>
#define MAX_PATH 260
#endif

static std::filesystem::path getResourcesPath()
{
	std::filesystem::path resourcesPath{};

#ifdef _MSC_VER
	CHAR exeDirBuf[MAX_PATH + 1];
	GetModuleFileNameA(NULL, exeDirBuf, MAX_PATH + 1);
	std::filesystem::path cwd = std::filesystem::path(exeDirBuf).parent_path();
	(void)_chdir((const char*)std::filesystem::absolute(cwd).c_str());
#else
	std::filesystem::path cwd = std::filesystem::current_path();
#endif

	if (std::filesystem::is_directory(cwd / "res")) {
		resourcesPath = cwd / "res";
	}
	else {
		resourcesPath = cwd.parent_path() / "share" / "sdltest";
	}

	if (std::filesystem::is_directory(resourcesPath) == false) {
		resourcesPath = cwd.root_path() / "usr" / "local" / "share" / "sdltest";
	}

	if (std::filesystem::is_directory(resourcesPath) == false) {
		throw std::runtime_error("Unable to determine resources location. CWD: " + cwd.string());
	}

	return resourcesPath;
}

namespace engine {

	Application::Application(const char* appName, const char* appVersion)
	{
		m_window = std::make_unique<Window>(appName, true, false);
		m_gfx = std::make_unique<GFXDevice>(appName, appVersion, m_window->getHandle());
		m_inputManager = std::make_unique<InputManager>(window());
		m_sceneManager = std::make_unique<SceneManager>(this);

		// get base path for resources
		m_resourcesPath = getResourcesPath();

		// register resource managers
		registerResourceManager<resources::Texture>();
		registerResourceManager<resources::Shader>();
		registerResourceManager<resources::Material>();
		registerResourceManager<resources::Mesh>();

		// default resources
		{
			resources::Shader::VertexParams vertParams{};
			vertParams.hasNormal = true;
			vertParams.hasUV0 = true;
			auto texturedShader = std::make_unique<resources::Shader>(
				gfx(),
				getResourcePath("engine/shaders/textured.vert").c_str(),
				getResourcePath("engine/shaders/textured.frag").c_str(),
				vertParams,
				false,
				true
			);
			getResourceManager<resources::Shader>()->addPersistent("engine.textured", std::move(texturedShader));
		}
		{
			resources::Shader::VertexParams vertParams{};
			vertParams.hasNormal = true;
			vertParams.hasUV0 = true;
			auto texturedShader = std::make_unique<resources::Shader>(
				gfx(),
				getResourcePath("engine/shaders/skybox.vert").c_str(),
				getResourcePath("engine/shaders/skybox.frag").c_str(),
				vertParams,
				false,
				true
			);
			getResourceManager<resources::Shader>()->addPersistent("engine.skybox", std::move(texturedShader));
		}
		{
			auto whiteTexture = std::make_unique<resources::Texture>(
				gfx(),
				getResourcePath("engine/textures/white.png"),
				resources::Texture::Filtering::OFF,
				false,
				false
			);
			getResourceManager<resources::Texture>()->addPersistent("engine.white", std::move(whiteTexture));
		}
	}

	Application::~Application() {}

	void Application::gameLoop()
	{
		TRACE("Begin game loop...");

		constexpr int FPS_LIMIT = 240;
		constexpr auto FRAMETIME_LIMIT = std::chrono::nanoseconds(1000000000 / FPS_LIMIT);
		auto beginFrame = std::chrono::steady_clock::now();
		auto endFrame = beginFrame + FRAMETIME_LIMIT;

		auto lastTick = m_window->getNanos();

		// single-threaded game loop
		while (m_window->isRunning()) {

			/* logic */
			m_sceneManager->updateActiveScene(m_window->dt());

			if(m_window->getKeyPress(inputs::Key::K_F)) [[unlikely]] {
				m_window->infoBox("fps", std::to_string(m_window->getFPS()) + " fps " + std::to_string(m_window->dt() * 1000.0f) + " ms");
			}

			uint64_t now = m_window->getNanos();
			if (now - lastTick >= 1000000000LL * 5LL) [[unlikely]] {
				lastTick = now;
				INFO("fps: {}", m_window->getAvgFPS());
				m_window->resetAvgFPS();
			}

			/* draw */
			m_gfx->renderFrame();

			/* poll events */
			m_window->getInputAndEvents();

			/* fps limiter */
			if (m_enableFrameLimiter) {
				std::this_thread::sleep_until(endFrame);
			}
			beginFrame = endFrame;
			endFrame = beginFrame + FRAMETIME_LIMIT;

		}

		m_gfx->waitIdle();
	}

}
