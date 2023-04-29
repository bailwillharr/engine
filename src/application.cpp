#include "application.hpp"

#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

#include <glm/mat4x4.hpp>

#include "gfx.hpp"
#include "gfx_device.hpp"
#include "input_manager.hpp"
#include "log.hpp"
#include "resources/material.hpp"
#include "resources/mesh.hpp"
#include "resources/shader.hpp"
#include "resources/texture.hpp"
#include "scene.hpp"
#include "scene_manager.hpp"
#include "window.hpp"

#ifdef _MSC_VER
#include <windows.h>
#include <direct.h>
#define WIN_MAX_PATH 260
#endif

namespace engine {

	static std::filesystem::path getResourcesPath()
	{
		std::filesystem::path resourcesPath{};

#ifdef _MSC_VER
		CHAR exeDirBuf[MAX_PATH + 1];
		GetModuleFileNameA(NULL, exeDirBuf, WIN_MAX_PATH + 1);
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
		window_ = std::make_unique<Window>(appName, true, false);
		input_manager_ = std::make_unique<InputManager>(window_.get());
		scene_manager_ = std::make_unique<SceneManager>(this);

		// get base path for resources
		resources_path_ = getResourcesPath();

		// register resource managers
		RegisterResourceManager<resources::Texture>();
		RegisterResourceManager<resources::Shader>();
		RegisterResourceManager<resources::Material>();
		RegisterResourceManager<resources::Mesh>();

		// initialise the render data
		render_data_.gfxdev = std::make_unique<GFXDevice>(appName, appVersion, window_->getHandle(), graphicsSettings);

		std::vector<gfx::DescriptorSetLayoutBinding> globalSetBindings;
		{
			auto& binding0 = globalSetBindings.emplace_back();
			binding0.descriptor_type = gfx::DescriptorType::kUniformBuffer;
			binding0.stage_flags = gfx::ShaderStageFlags::kVertex;
		}
		render_data_.global_set_layout = gfxdev()->CreateDescriptorSetLayout(globalSetBindings);
		render_data_.global_set = gfxdev()->AllocateDescriptorSet(render_data_.global_set_layout);
		RenderData::GlobalSetUniformBuffer globalSetUniformBufferData{
			.proj = glm::mat4{ 1.0f },
		};
		render_data_.global_set_uniform_buffer = gfxdev()->CreateUniformBuffer(sizeof(RenderData::GlobalSetUniformBuffer), &globalSetUniformBufferData);
		gfxdev()->UpdateDescriptorUniformBuffer(render_data_.global_set, 0, render_data_.global_set_uniform_buffer, 0, sizeof(RenderData::GlobalSetUniformBuffer));

		std::vector<gfx::DescriptorSetLayoutBinding> frameSetBindings;
		{
			auto& binding0 = frameSetBindings.emplace_back();
			binding0.descriptor_type = gfx::DescriptorType::kUniformBuffer;
			binding0.stage_flags = gfx::ShaderStageFlags::kVertex;
		}
		render_data_.frame_set_layout = gfxdev()->CreateDescriptorSetLayout(frameSetBindings);
		render_data_.frame_set = gfxdev()->AllocateDescriptorSet(render_data_.frame_set_layout);
		RenderData::FrameSetUniformBuffer initialSetOneData{
			.view = glm::mat4{ 1.0f },
		};
		render_data_.frame_set_uniform_buffer = gfxdev()->CreateUniformBuffer(sizeof(RenderData::FrameSetUniformBuffer), &initialSetOneData);
		gfxdev()->UpdateDescriptorUniformBuffer(render_data_.frame_set, 0, render_data_.frame_set_uniform_buffer, 0, sizeof(RenderData::FrameSetUniformBuffer));

		std::vector<gfx::DescriptorSetLayoutBinding> materialSetBindings;
		{
			auto& binding0 = materialSetBindings.emplace_back();
			binding0.descriptor_type = gfx::DescriptorType::kCombinedImageSampler;
			binding0.stage_flags = gfx::ShaderStageFlags::kFragment;
		}
		render_data_.material_set_layout = gfxdev()->CreateDescriptorSetLayout(materialSetBindings);

		// default resources
		{
			resources::Shader::VertexParams vertParams{};
			vertParams.hasNormal = true;
			vertParams.hasUV0 = true;
			auto texturedShader = std::make_unique<resources::Shader>(
				&render_data_,
				GetResourcePath("engine/shaders/standard.vert").c_str(),
				GetResourcePath("engine/shaders/standard.frag").c_str(),
				vertParams,
				false,
				true
			);
			GetResourceManager<resources::Shader>()->AddPersistent("builtin.standard", std::move(texturedShader));
		}
		{
			resources::Shader::VertexParams vertParams{};
			vertParams.hasNormal = true;
			vertParams.hasUV0 = true;
			auto skyboxShader = std::make_unique<resources::Shader>(
				&render_data_,
				GetResourcePath("engine/shaders/skybox.vert").c_str(),
				GetResourcePath("engine/shaders/skybox.frag").c_str(),
				vertParams,
				false,
				true
			);
			GetResourceManager<resources::Shader>()->AddPersistent("builtin.skybox", std::move(skyboxShader));
		}
		{
			auto whiteTexture = std::make_unique<resources::Texture>(
				&render_data_,
				GetResourcePath("engine/textures/white.png"),
				resources::Texture::Filtering::OFF
			);
			GetResourceManager<resources::Texture>()->AddPersistent("builtin.white", std::move(whiteTexture));
		}
	}

	Application::~Application()
	{
		for (const auto& [info, sampler] : render_data_.samplers) {
			gfxdev()->DestroySampler(sampler);
		}
		gfxdev()->DestroyDescriptorSetLayout(render_data_.material_set_layout);

		gfxdev()->DestroyUniformBuffer(render_data_.frame_set_uniform_buffer);
		gfxdev()->DestroyDescriptorSetLayout(render_data_.frame_set_layout);

		gfxdev()->DestroyUniformBuffer(render_data_.global_set_uniform_buffer);
		gfxdev()->DestroyDescriptorSetLayout(render_data_.global_set_layout);
	}

	void Application::GameLoop()
	{
		LOG_TRACE("Begin game loop...");

		constexpr int FPS_LIMIT = 240;
		constexpr auto FRAMETIME_LIMIT = std::chrono::nanoseconds(1000000000 / FPS_LIMIT);
		auto beginFrame = std::chrono::steady_clock::now();
		auto endFrame = beginFrame + FRAMETIME_LIMIT;

		auto lastTick = window_->getNanos();

		// single-threaded game loop
		while (window_->isRunning()) {

			/* logic */
			scene_manager_->UpdateActiveScene(window_->dt());

			if(window_->getKeyPress(inputs::Key::K_F)) [[unlikely]] {
				window_->infoBox("fps", std::to_string(window_->getFPS()) + " fps " + std::to_string(window_->dt() * 1000.0f) + " ms");
			}

			uint64_t now = window_->getNanos();
			if (now - lastTick >= 1000000000LL * 5LL) [[unlikely]] {
				lastTick = now;
				LOG_INFO("fps: {}", window_->getAvgFPS());
				gfxdev()->LogPerformanceInfo();
				window_->resetAvgFPS();
			}

			/* poll events */
			window_->getInputAndEvents();

			/* fps limiter */
			if (enable_frame_limiter_) {
				std::this_thread::sleep_until(endFrame);
			}
			beginFrame = endFrame;
			endFrame = beginFrame + FRAMETIME_LIMIT;

		}

		gfxdev()->WaitIdle();
	}

}
