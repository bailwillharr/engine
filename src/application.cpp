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
#include "systems/mesh_render_system.h"
#include "components/transform.h"
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
  RegisterResourceManager<resources::Mesh>();
  RegisterResourceManager<resources::Material>();
  RegisterResourceManager<resources::Texture>();
  RegisterResourceManager<resources::Shader>();
  RegisterResourceManager<resources::Font>();

  renderer_ = std::make_unique<Renderer>(
      appName, appVersion, window_->GetHandle(), graphicsSettings);

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
        renderer(), GetResourcePath("engine/shaders/standard.vert").c_str(),
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
        renderer(), GetResourcePath("engine/shaders/skybox.vert").c_str(),
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
        renderer(), GetResourcePath("engine/shaders/quad.vert").c_str(),
        GetResourcePath("engine/shaders/quad.frag").c_str(), shaderSettings);
    GetResourceManager<resources::Shader>()->AddPersistent(
        "builtin.quad", std::move(quadShader));
  }

  /* default textures */
  {
    auto whiteTexture = std::make_unique<resources::Texture>(
        renderer(), GetResourcePath("engine/textures/white.png"),
        resources::Texture::Filtering::kOff);
    GetResourceManager<resources::Texture>()->AddPersistent(
        "builtin.white", std::move(whiteTexture));
  }
}

Application::~Application() {}

void Application::GameLoop() {
  LOG_DEBUG("Begin game loop...");

  constexpr int FPS_LIMIT = 240;
  constexpr auto FRAMETIME_LIMIT =
      std::chrono::nanoseconds(1000000000 / FPS_LIMIT);
  auto beginFrame = std::chrono::steady_clock::now();
  auto endFrame = beginFrame + FRAMETIME_LIMIT;

  auto lastTick = window_->GetNanos();

  // single-threaded game loop
  while (window_->IsRunning()) {
    /* logic */
    Scene* scene = scene_manager_->UpdateActiveScene(window_->dt());

    uint64_t now = window_->GetNanos();
    if (now - lastTick >= 1000000000LL * 5LL) [[unlikely]] {
      lastTick = now;
      LOG_INFO("fps: {}", window_->GetAvgFPS());
      // renderer()->GetDevice()->LogPerformanceInfo();
      window_->ResetAvgFPS();
    }

    /* render */
    renderer_->PreRender(window()->GetWindowResized(), scene->GetComponent<TransformComponent>(scene->GetEntity("camera"))->world_matrix);

    if (scene) {
      auto mesh_render_system = scene->GetSystem<MeshRenderSystem>();
      const RenderList* static_list = mesh_render_system->GetStaticRenderList();
      const RenderList* dynamic_list = mesh_render_system->GetDynamicRenderList();
      renderer_->Render(*static_list, *dynamic_list);
    }

    /* poll events */
    window_->GetInputAndEvents();

    /* fps limiter */
    if (enable_frame_limiter_) {
      std::this_thread::sleep_until(endFrame);
    }
    beginFrame = endFrame;
    endFrame = beginFrame + FRAMETIME_LIMIT;
  }

  renderer_->GetDevice()->WaitIdle();
}

}  // namespace engine
