#include "application.h"

#include <cinttypes>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <numeric>

#include <glm/mat4x4.hpp>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_sdl2.h"
#include "imgui/imgui_impl_vulkan.h"

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

static struct ImGuiThings {
    ImGuiContext* context;
} im_gui_things;

namespace engine {

static std::filesystem::path getResourcesPath()
{
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

Application::Application(const char* appName, const char* appVersion, gfx::GraphicsSettings graphicsSettings, Configuration configuration)
    : configuration_(configuration)
{
    window_ = std::make_unique<Window>(appName, true, false);
    input_manager_ = std::make_unique<InputManager>(window_.get());
    scene_manager_ = std::make_unique<SceneManager>(this);

    // get base path for resources
    resources_path_ = getResourcesPath();

    // register resource managers
    RegisterResourceManager<Mesh>();
    RegisterResourceManager<Material>();
    RegisterResourceManager<Texture>();
    RegisterResourceManager<Shader>();
    RegisterResourceManager<Font>();

    im_gui_things.context = ImGui::CreateContext();
    // ImGuiIO& io = ImGui::GetIO()
    ImGui_ImplSDL2_InitForVulkan(window_->GetHandle());

    renderer_ = std::make_unique<Renderer>(appName, appVersion, window_->GetHandle(), graphicsSettings);

    /* default fonts */
    {
        auto monoFont = std::make_unique<Font>(GetResourcePath("engine/fonts/mono.ttf"));
        GetResourceManager<Font>()->AddPersistent("builtin.mono", std::move(monoFont));
    }

    /* default shaders */
    {
        Shader::VertexParams vertParams{};
        vertParams.has_normal = true;
        vertParams.has_tangent = true;
        vertParams.has_uv0 = true;
        Shader::ShaderSettings shaderSettings{};
        shaderSettings.vertexParams = vertParams;
        shaderSettings.alpha_blending = false;
        shaderSettings.cull_backface = true;
        shaderSettings.write_z = true;
        shaderSettings.render_order = 0;
        auto texturedShader = std::make_unique<Shader>(renderer(), GetResourcePath("engine/shaders/standard.vert").c_str(),
                                                       GetResourcePath("engine/shaders/standard.frag").c_str(), shaderSettings);
        GetResourceManager<Shader>()->AddPersistent("builtin.standard", std::move(texturedShader));
    }
    {
        Shader::VertexParams vertParams{};
        vertParams.has_normal = true;
        vertParams.has_tangent = true;
        vertParams.has_uv0 = true;
        Shader::ShaderSettings shaderSettings{};
        shaderSettings.vertexParams = vertParams;
        shaderSettings.alpha_blending = false;
        shaderSettings.cull_backface = true;
        shaderSettings.write_z = true;
        shaderSettings.render_order = 0;
        auto fancyShader = std::make_unique<Shader>(renderer(), GetResourcePath("engine/shaders/fancy.vert").c_str(),
                                                    GetResourcePath("engine/shaders/fancy.frag").c_str(), shaderSettings);
        GetResourceManager<Shader>()->AddPersistent("builtin.fancy", std::move(fancyShader));
    }
    {
        Shader::VertexParams vertParams{};
        vertParams.has_normal = true;
        vertParams.has_tangent = true;
        vertParams.has_uv0 = true;
        Shader::ShaderSettings shaderSettings{};
        shaderSettings.vertexParams = vertParams;
        shaderSettings.alpha_blending = false;
        shaderSettings.cull_backface = true;
        shaderSettings.write_z = true;
        shaderSettings.render_order = 0;
        auto skyboxShader = std::make_unique<Shader>(renderer(), GetResourcePath("engine/shaders/skybox.vert").c_str(),
                                                     GetResourcePath("engine/shaders/skybox.frag").c_str(), shaderSettings);
        GetResourceManager<Shader>()->AddPersistent("builtin.skybox", std::move(skyboxShader));
    }
#if 0
    {
        Shader::VertexParams vertParams{};
        vertParams.has_normal = true;
        vertParams.has_tangent = true;
        vertParams.has_uv0 = true;
        Shader::ShaderSettings shaderSettings{};
        shaderSettings.vertexParams = vertParams;
        shaderSettings.alpha_blending = true;
        shaderSettings.cull_backface = true;
        shaderSettings.write_z = false;
        shaderSettings.render_order = 1;
        auto quadShader = std::make_unique<Shader>(renderer(), GetResourcePath("engine/shaders/quad.vert").c_str(),
                                                              GetResourcePath("engine/shaders/quad.frag").c_str(), shaderSettings);
        GetResourceManager<Shader>()->AddPersistent("builtin.quad", std::move(quadShader));
    }
#endif

    /* default textures */
    {
        auto whiteTexture = LoadTextureFromFile(GetResourcePath("engine/textures/white.png"), gfx::SamplerInfo{}, renderer(), true);
        GetResourceManager<Texture>()->AddPersistent("builtin.white", std::move(whiteTexture));
    }
    {
        auto normalTexture = LoadTextureFromFile(GetResourcePath("engine/textures/normal.png"), gfx::SamplerInfo{}, renderer(), false);
        GetResourceManager<Texture>()->AddPersistent("builtin.normal", std::move(normalTexture));
    }

    /* default materials */
    {
        auto defaultMaterial = std::make_unique<Material>(renderer(), GetResource<Shader>("builtin.fancy"));
        defaultMaterial->SetAlbedoTexture(GetResource<Texture>("builtin.white"));
        defaultMaterial->SetNormalTexture(GetResource<Texture>("builtin.normal"));
        GetResourceManager<Material>()->AddPersistent("builtin.default", std::move(defaultMaterial));
    }
}

Application::~Application()
{
    renderer_->GetDevice()->ShutdownImguiBackend();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext(im_gui_things.context);
}

void Application::GameLoop()
{
    LOG_DEBUG("Begin game loop...");

    constexpr int FPS_LIMIT = 240;
    constexpr auto FRAMETIME_LIMIT = std::chrono::nanoseconds(1000000000 / FPS_LIMIT);
    auto beginFrame = std::chrono::steady_clock::now();
    auto endFrame = beginFrame + FRAMETIME_LIMIT;

    auto lastTick = window_->GetNanos();
    std::array<float, 20> delta_times{};

    struct DebugMenuState {
        bool menu_active = false;
        bool show_info_window = true;
    } debug_menu_state;

    // single-threaded game loop
    while (window_->IsRunning()) {
        /* logic */

        const float avg_fps = static_cast<float>(delta_times.size()) / std::accumulate(delta_times.begin(), delta_times.end(), 0.0f);

        Scene* scene = scene_manager_->UpdateActiveScene(window_->dt());

        uint64_t now = window_->GetNanos();
        if (now - lastTick >= 1000000000LL * 5LL) [[unlikely]] {
            lastTick = now;
            LOG_INFO("fps: {}", std::lroundf(avg_fps));
            // renderer()->GetDevice()->LogPerformanceInfo();
            window_->ResetAvgFPS();
        }

        if (window_->GetKeyPress(inputs::Key::K_F5)) {
            bool show_window = window_->MouseCaptured();
            debug_menu_state.menu_active = show_window;
            window_->SetRelativeMouseMode(!show_window);
        }

        if (window_->GetKeyPress(inputs::Key::K_F6)) {
            debug_menu_state.show_info_window = !debug_menu_state.show_info_window;
        }

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        if (debug_menu_state.menu_active) {
            if (ImGui::Begin("debugMenu", 0)) {
                ImGui::Text("Test!");
                ImGui::Text("FPS: %.3f", std::roundf(avg_fps));
            }
            ImGui::End();
        }

        if (debug_menu_state.show_info_window) {
            if (ImGui::Begin(
                    "infoWindow", nullptr,
                    ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoFocusOnAppearing)) {
                ImGui::Text("Scene hierarchy:");

                std::function<int(Entity, int)> find_depth = [&](Entity e, int current_depth) -> int {
                    Entity parent = scene->GetComponent<TransformComponent>(e)->parent;
                    if (parent == 0)
                        return current_depth;
                    else {
                        return find_depth(parent, current_depth + 1);
                    }
                };
                if (scene) {
                    for (Entity i = 1; i < scene->next_entity_id_; ++i) {
                        auto t = scene->GetComponent<TransformComponent>(i);
                        std::string tabs{};
                        int depth = find_depth(i, 0);
                        for (int j = 0; j < depth; ++j) tabs += std::string{"    "};
                        ImGui::Text("%s%s", tabs.c_str(), t->tag.c_str());
                        ImGui::Text("%.1f %.1f %.1f", t->position.x, t->position.y, t->position.z);
                    }
                }
                else {
                    ImGui::Text("No scene active!");
                }
            }
            ImGui::End();
        }

        ImGui::Render();

        const RenderList* static_list = nullptr;
        const RenderList* dynamic_list = nullptr;
        glm::mat4 camera_transform{1.0f};
        if (scene) {
            camera_transform = scene->GetComponent<TransformComponent>(scene->GetEntity("camera"))->world_matrix;
            auto mesh_render_system = scene->GetSystem<MeshRenderSystem>();
            static_list = mesh_render_system->GetStaticRenderList();
            dynamic_list = mesh_render_system->GetDynamicRenderList();
        }
        renderer_->PreRender(window()->GetWindowResized(), camera_transform);
        renderer_->Render(static_list, dynamic_list);

        /* poll events */
        window_->GetInputAndEvents();

        /* fps limiter */
        if (configuration_.enable_frame_limiter) {
            std::this_thread::sleep_until(endFrame);
        }
        beginFrame = endFrame;
        endFrame = beginFrame + FRAMETIME_LIMIT;
        delta_times[window_->GetFrameCount() % delta_times.size()] = window_->dt();
    }

    renderer_->GetDevice()->WaitIdle();
}

} // namespace engine
