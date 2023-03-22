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

#include <glm/gtc/matrix_transform.hpp>

// To allow the FPS-limiter to put the thread to sleep
#include <thread>

#ifdef _MSC_VER
#include <windows.h>
#include <direct.h>
#define MAX_PATH 260
#endif

namespace engine {

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

	Application::Application(const char* appName, const char* appVersion, gfx::GraphicsSettings graphicsSettings)
	{
		m_window = std::make_unique<Window>(appName, true, false);
		m_inputManager = std::make_unique<InputManager>(window());
		m_sceneManager = std::make_unique<SceneManager>(this);

		// get base path for resources
		m_resourcesPath = getResourcesPath();

		// register resource managers
		registerResourceManager<resources::Texture>();
		registerResourceManager<resources::Shader>();
		registerResourceManager<resources::Material>();
		registerResourceManager<resources::Mesh>();

		// initialise the render data
		renderData.gfxdev = std::make_unique<GFXDevice>(appName, appVersion, m_window->getHandle(), graphicsSettings);

		std::vector<gfx::DescriptorSetLayoutBinding> setZeroLayoutBindings;
		{
			auto& binding0 = setZeroLayoutBindings.emplace_back();
			binding0.descriptorType = gfx::DescriptorType::UNIFORM_BUFFER;
			binding0.stageFlags = gfx::ShaderStageFlags::VERTEX;
			auto& binding1 = setZeroLayoutBindings.emplace_back();
			binding1.descriptorType = gfx::DescriptorType::COMBINED_IMAGE_SAMPLER;
			binding1.stageFlags = gfx::ShaderStageFlags::FRAGMENT;
		}
		renderData.setZeroLayout = gfx()->createDescriptorSetLayout(setZeroLayoutBindings);
		renderData.setZero = gfx()->allocateDescriptorSet(renderData.setZeroLayout);
		RenderData::SetZeroBuffer initialSetZeroData{
			.proj = glm::perspectiveZO(glm::radians(70.0f), 1024.0f / 768.0f, 0.1f, 1000.0f),
		};
		renderData.setZeroBuffer = gfx()->createUniformBuffer(sizeof(RenderData::SetZeroBuffer), &initialSetZeroData);
		gfx()->updateDescriptorUniformBuffer(renderData.setZero, 0, renderData.setZeroBuffer, 0, sizeof(RenderData::SetZeroBuffer));

		uint8_t* imageData = new uint8_t[16*16*4];
		for (int i = 0; i < 16*16*4; i += 4) {
			imageData[i + 0] = ((i / 4u) % 16u) * 16u;
			imageData[i + 1] = (i / 4u) & 0xF0u;
			imageData[i + 2] = 0x00;
			imageData[i + 3] = 0xFF;
		}
		renderData.myImage = gfx()->createImage(16, 16, imageData);
		delete[] imageData;

		renderData.mySampler = gfx()->createSampler();
		gfx()->updateDescriptorCombinedImageSampler(renderData.setZero, 1, renderData.myImage, renderData.mySampler);

		std::vector<gfx::DescriptorSetLayoutBinding> setOneLayoutBindings;
		{
			auto& binding0 = setOneLayoutBindings.emplace_back();
			binding0.descriptorType = gfx::DescriptorType::UNIFORM_BUFFER;
			binding0.stageFlags = gfx::ShaderStageFlags::VERTEX;
		}
		renderData.setOneLayout = gfx()->createDescriptorSetLayout(setOneLayoutBindings);
		renderData.setOne = gfx()->allocateDescriptorSet(renderData.setOneLayout);
		RenderData::SetOneBuffer initialSetOneData{
			.view = glm::mat4{ 1.0f },
		};
		renderData.setOneBuffer = gfx()->createUniformBuffer(sizeof(RenderData::SetOneBuffer), &initialSetOneData);
		gfx()->updateDescriptorUniformBuffer(renderData.setOne, 0, renderData.setOneBuffer, 0, sizeof(RenderData::SetOneBuffer));

		// default resources
		{
			resources::Shader::VertexParams vertParams{};
			vertParams.hasNormal = true;
			vertParams.hasUV0 = true;
			auto texturedShader = std::make_unique<resources::Shader>(
				&renderData,
				getResourcePath("engine/shaders/standard.vert").c_str(),
				getResourcePath("engine/shaders/standard.frag").c_str(),
				vertParams,
				false,
				true
			);
			getResourceManager<resources::Shader>()->addPersistent("builtin.standard", std::move(texturedShader));
		}
		if (0) {
			resources::Shader::VertexParams vertParams{};
			vertParams.hasNormal = true;
			vertParams.hasUV0 = true;
			auto texturedShader = std::make_unique<resources::Shader>(
				&renderData,
				getResourcePath("engine/shaders/skybox.vert").c_str(),
				getResourcePath("engine/shaders/skybox.frag").c_str(),
				vertParams,
				false,
				true
			);
			getResourceManager<resources::Shader>()->addPersistent("builtin.skybox", std::move(texturedShader));
		}
		{
			auto whiteTexture = std::make_unique<resources::Texture>(
				gfx(),
				getResourcePath("engine/textures/white.png"),
				resources::Texture::Filtering::OFF,
				false,
				false
			);
			getResourceManager<resources::Texture>()->addPersistent("builtin.white", std::move(whiteTexture));
		}
	}

	Application::~Application()
	{
		gfx()->destroyUniformBuffer(renderData.setOneBuffer);
		gfx()->destroyDescriptorSetLayout(renderData.setOneLayout);

		gfx()->destroySampler(renderData.mySampler);
		gfx()->destroyImage(renderData.myImage);
		gfx()->destroyUniformBuffer(renderData.setZeroBuffer);
		gfx()->destroyDescriptorSetLayout(renderData.setZeroLayout);
	}

	void Application::gameLoop()
	{
		LOG_TRACE("Begin game loop...");

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
				LOG_INFO("fps: {}", m_window->getAvgFPS());
				gfx()->logPerformanceInfo();
				m_window->resetAvgFPS();
			}

			/* poll events */
			m_window->getInputAndEvents();

			/* fps limiter */
			if (m_enableFrameLimiter) {
				std::this_thread::sleep_until(endFrame);
			}
			beginFrame = endFrame;
			endFrame = beginFrame + FRAMETIME_LIMIT;

		}

		gfx()->waitIdle();
	}

}
