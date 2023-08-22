#include "application.h"

#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

#include <glm/mat4x4.hpp>

#include "gfx.h"
#include "gfx_device.h"
#include "input_manager.h"
#include "log.h"
#include "resources/font.h"
#include "resources/material.h"
#include "resources/mesh.h"
#include "resources/shader.h"
#include "resources/texture.h"
#include "scene.h"
#include "scene_manager.h"
#include "window.h"

#ifdef _MSC_VER
#include <windows.h>
#include <direct.h>
#define WIN_MAX_PATH 260
#endif

namespace engine {

static std::filesystem::path getResourcesPath() {
  std::filesystem::path resourcesPath{};

#ifdef _MSC_VER
  // get the path of the currently running process
  CHAR exeDirBuf[MAX_PATH + 1];
  GetModuleFileNameA(NULL, exeDirBuf, WIN_MAX_PATH + 1);
  std::filesystem::path cwd = std::filesystem::path(exeDirBuf).parent_path();
  (void)_chdir((const char*)std::filesystem::absolute(cwd).c_str());
#else
  std::filesystem::path cwd = std::filesystem::current_path();
#endif

  if (std::filesystem::is_directory(cwd / "res")) {
    resourcesPath = cwd / "res";
  } else {
    resourcesPath = cwd.parent_path() / "share" / "sdltest";
  }

  if (std::filesystem::is_directory(resourcesPath) == false) {
    resourcesPath = cwd.root_path() / "usr" / "local" / "share" / "sdltest";
  }

  if (std::filesystem::is_directory(resourcesPath) == false) {
    throw std::runtime_error("Unable to determine resources location. CWD: " +
                             cwd.string());
  }

  return resourcesPath;
}

Application::Application(const char* appName, const char* appVersion,
                         gfx::GraphicsSettings graphicsSettings) {
  window_ = std::make_unique<Window>(appName, true, false);
  input_manager_ = std::make_unique<InputManager>(window_.get());
  scene_manager_ = std::make_unique<SceneManager>(this);

  // get base path for resources
  resources_path_ = getResourcesPath();

  // register resource managers
  RegisterResourceManager<resources::Font>();
  RegisterResourceManager<resources::Texture>();
  RegisterResourceManager<resources::Shader>();
  RegisterResourceManager<resources::Material>();
  RegisterResourceManager<resources::Mesh>();

  // initialise the render data
  render_data_.gfxdev = std::make_unique<GFXDevice>(
      appName, appVersion, window_->GetHandle(), graphicsSettings);

  std::vector<gfx::DescriptorSetLayoutBinding> globalSetBindings;
  {
    auto& binding0 = globalSetBindings.emplace_back();
    binding0.descriptor_type = gfx::DescriptorType::kUniformBuffer;
    binding0.stage_flags = gfx::ShaderStageFlags::kVertex;
  }
  render_data_.global_set_layout =
      gfxdev()->CreateDescriptorSetLayout(globalSetBindings);
  render_data_.global_set =
      gfxdev()->AllocateDescriptorSet(render_data_.global_set_layout);
  RenderData::GlobalSetUniformBuffer globalSetUniformBufferData{
      .proj = glm::mat4{1.0f},
  };
  render_data_.global_set_uniform_buffer = gfxdev()->CreateUniformBuffer(
      sizeof(RenderData::GlobalSetUniformBuffer), &globalSetUniformBufferData);
  gfxdev()->UpdateDescriptorUniformBuffer(
      render_data_.global_set, 0, render_data_.global_set_uniform_buffer, 0,
      sizeof(RenderData::GlobalSetUniformBuffer));

  std::vector<gfx::DescriptorSetLayoutBinding> frameSetBindings;
  {
    auto& binding0 = frameSetBindings.emplace_back();
    binding0.descriptor_type = gfx::DescriptorType::kUniformBuffer;
    binding0.stage_flags = gfx::ShaderStageFlags::kVertex;
  }
  render_data_.frame_set_layout =
      gfxdev()->CreateDescriptorSetLayout(frameSetBindings);
  render_data_.frame_set =
      gfxdev()->AllocateDescriptorSet(render_data_.frame_set_layout);
  RenderData::FrameSetUniformBuffer initialSetOneData{
      .view = glm::mat4{1.0f},
  };
  render_data_.frame_set_uniform_buffer = gfxdev()->CreateUniformBuffer(
      sizeof(RenderData::FrameSetUniformBuffer), &initialSetOneData);
  gfxdev()->UpdateDescriptorUniformBuffer(
      render_data_.frame_set, 0, render_data_.frame_set_uniform_buffer, 0,
      sizeof(RenderData::FrameSetUniformBuffer));

  std::vector<gfx::DescriptorSetLayoutBinding> materialSetBindings;
  {
    auto& binding0 = materialSetBindings.emplace_back();
    binding0.descriptor_type = gfx::DescriptorType::kCombinedImageSampler;
    binding0.stage_flags = gfx::ShaderStageFlags::kFragment;
  }
  render_data_.material_set_layout =
      gfxdev()->CreateDescriptorSetLayout(materialSetBindings);

  /* default fonts */
  {
    auto monoFont = std::make_unique<resources::Font>(
        GetResourcePath("engine/fonts/mono.ttf"));
    GetResourceManager<resources::Font>()->AddPersistent("builtin.mono",
                                                         std::move(monoFont));
  }

  /* default shaders */
  {
    resources::Shader::VertexParams vertParams{};
    vertParams.has_normal = true;
    vertParams.has_uv0 = true;
    resources::Shader::ShaderSettings shaderSettings{};
    shaderSettings.vertexParams = vertParams;
    shaderSettings.alpha_blending = false;
    shaderSettings.cull_backface = true;
    shaderSettings.write_z = true;
    shaderSettings.render_order = 0;
    auto texturedShader = std::make_unique<resources::Shader>(
        &render_data_, GetResourcePath("engine/shaders/standard.vert").c_str(),
        GetResourcePath("engine/shaders/standard.frag").c_str(),
        shaderSettings);
    GetResourceManager<resources::Shader>()->AddPersistent(
        "builtin.standard", std::move(texturedShader));
  }
  {
    resources::Shader::VertexParams vertParams{};
    vertParams.has_normal = true;
    vertParams.has_uv0 = true;
    resources::Shader::ShaderSettings shaderSettings{};
    shaderSettings.vertexParams = vertParams;
    shaderSettings.alpha_blending = false;
    shaderSettings.cull_backface = true;
    shaderSettings.write_z = true;
    shaderSettings.render_order = 0;
    auto skyboxShader = std::make_unique<resources::Shader>(
        &render_data_, GetResourcePath("engine/shaders/skybox.vert").c_str(),
        GetResourcePath("engine/shaders/skybox.frag").c_str(), shaderSettings);
    GetResourceManager<resources::Shader>()->AddPersistent(
        "builtin.skybox", std::move(skyboxShader));
  }
  {
    resources::Shader::VertexParams vertParams{};
    vertParams.has_normal = true;
    vertParams.has_uv0 = true;
    resources::Shader::ShaderSettings shaderSettings{};
    shaderSettings.vertexParams = vertParams;
    shaderSettings.alpha_blending = true;
    shaderSettings.cull_backface = true;
    shaderSettings.write_z = false;
    shaderSettings.render_order = 1;
    auto quadShader = std::make_unique<resources::Shader>(
        &render_data_, GetResourcePath("engine/shaders/quad.vert").c_str(),
        GetResourcePath("engine/shaders/quad.frag").c_str(), shaderSettings);
    GetResourceManager<resources::Shader>()->AddPersistent(
        "builtin.quad", std::move(quadShader));
  }

  /* default textures */
  {
    auto whiteTexture = std::make_unique<resources::Texture>(
        &render_data_, GetResourcePath("engine/textures/white.png"),
        resources::Texture::Filtering::kOff);
    GetResourceManager<resources::Texture>()->AddPersistent(
        "builtin.white", std::move(whiteTexture));
  }
}

Application::~Application() {
  for (const auto& [info, sampler] : render_data_.samplers) {
    gfxdev()->DestroySampler(sampler);
  }
  gfxdev()->DestroyDescriptorSetLayout(render_data_.material_set_layout);

  gfxdev()->DestroyUniformBuffer(render_data_.frame_set_uniform_buffer);
  gfxdev()->DestroyDescriptorSetLayout(render_data_.frame_set_layout);

  gfxdev()->DestroyUniformBuffer(render_data_.global_set_uniform_buffer);
  gfxdev()->DestroyDescriptorSetLayout(render_data_.global_set_layout);
}

void Application::GameLoop() {
  LOG_TRACE("Begin game loop...");

  constexpr int FPS_LIMIT = 240;
  constexpr auto FRAMETIME_LIMIT =
      std::chrono::nanoseconds(1000000000 / FPS_LIMIT);
  auto beginFrame = std::chrono::steady_clock::now();
  auto endFrame = beginFrame + FRAMETIME_LIMIT;

  auto lastTick = window_->GetNanos();

  // single-threaded game loop
  while (window_->IsRunning()) {
    /* logic */
    scene_manager_->UpdateActiveScene(window_->dt());

    uint64_t now = window_->GetNanos();
    if (now - lastTick >= 1000000000LL * 5LL) [[unlikely]] {
      lastTick = now;
      LOG_INFO("fps: {}", window_->GetAvgFPS());
      gfxdev()->LogPerformanceInfo();
      window_->ResetAvgFPS();
    }

    /* render */
    

    /* poll events */
    window_->GetInputAndEvents();

    /* fps limiter */
    if (enable_frame_limiter_) {
      std::this_thread::sleep_until(endFrame);
    }
    beginFrame = endFrame;
    endFrame = beginFrame + FRAMETIME_LIMIT;
  }

  gfxdev()->WaitIdle();
}

}  // namespace engine
