#include "application.h"

#include <filesystem>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <string>
#include <thread>

#include <glm/mat4x4.hpp>

#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>

#include "component_collider.h"
#include "component_transform.h"
#include "gfx.h"
#include "gfx_device.h"
#include "input_manager.h"
#include "log.h"
#include "resource_font.h"
#include "resource_material.h"
#include "resource_mesh.h"
#include "resource_shader.h"
#include "resource_texture.h"
#include "renderer.h"
#include "debug_line.h"
#include "scene.h"
#include "scene_manager.h"
#include "system_collisions.h"
#include "system_mesh_render.h"
#include "file_dialog.h"
#include "gltf_loader.h"
#include "window.h"

static struct ImGuiThings {
    ImGuiContext* context;
} im_gui_things;

namespace engine {

static std::filesystem::path getResourcesPath()
{
    std::filesystem::path resourcesPath(SDL_GetBasePath());

    resourcesPath /= "res";

    if (std::filesystem::is_directory(resourcesPath) == false) {
        throw std::runtime_error("Unable to find game resources directory");
    }

    return resourcesPath;
}

static auto frametimeFromFPS(int fps) { return std::chrono::nanoseconds(1'000'000'000 / fps); }

Application::Application(const char* appName, const char* appVersion, gfx::GraphicsSettings graphicsSettings, AppConfiguration configuration)
    : app_name(appName), app_version(appVersion), m_configuration(configuration)
{
    m_window = std::make_unique<Window>(appName, true, false);
    m_input_manager = std::make_unique<InputManager>(m_window.get());
    m_scene_manager = std::make_unique<SceneManager>(this);

    // get base path for resources
    m_resources_path = getResourcesPath();

    // register resource managers
    registerResourceManager<Mesh>();
    registerResourceManager<Material>();
    registerResourceManager<Texture>();
    registerResourceManager<Shader>();
    registerResourceManager<Font>();

    im_gui_things.context = ImGui::CreateContext();
    // ImGuiIO& io = ImGui::GetIO()
    ImGui_ImplSDL2_InitForVulkan(m_window->GetHandle());

    m_renderer = std::make_unique<Renderer>(*this, graphicsSettings);

    /* default fonts */
    {
        auto monoFont = std::make_unique<Font>(getResourcePath("engine/fonts/mono.ttf"));
        getResourceManager<Font>()->AddPersistent("builtin.mono", std::move(monoFont));
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
        auto fancyShader =
            std::make_unique<Shader>(getRenderer(), getResourcePath("engine/shaders/fancy.vert"), getResourcePath("engine/shaders/fancy.frag"), shaderSettings);
        getResourceManager<Shader>()->AddPersistent("builtin.fancy", std::move(fancyShader));
    }

    /* default textures */
    {
        const uint8_t pixel[4] = {255, 255, 255, 255};
        gfx::SamplerInfo samplerInfo{};
        samplerInfo.minify = gfx::Filter::kNearest;
        samplerInfo.magnify = gfx::Filter::kNearest;
        samplerInfo.mipmap = gfx::Filter::kNearest;
        samplerInfo.anisotropic_filtering = false;
        auto whiteTexture = std::make_unique<Texture>(getRenderer(), pixel, 1, 1, samplerInfo, true);
        getResourceManager<Texture>()->AddPersistent("builtin.white", std::move(whiteTexture));
    }
    {
        const uint8_t pixel[4] = {0, 0, 0, 255};
        gfx::SamplerInfo samplerInfo{};
        samplerInfo.minify = gfx::Filter::kNearest;
        samplerInfo.magnify = gfx::Filter::kNearest;
        samplerInfo.mipmap = gfx::Filter::kNearest;
        samplerInfo.anisotropic_filtering = false;
        auto blackTexture = std::make_unique<Texture>(getRenderer(), pixel, 1, 1, samplerInfo, true);
        getResourceManager<Texture>()->AddPersistent("builtin.black", std::move(blackTexture));
    }
    {
        const uint8_t pixel[4] = {127, 127, 255, 255};
        gfx::SamplerInfo samplerInfo{};
        samplerInfo.minify = gfx::Filter::kNearest;
        samplerInfo.magnify = gfx::Filter::kNearest;
        samplerInfo.mipmap = gfx::Filter::kNearest;
        samplerInfo.anisotropic_filtering = false;
        auto normalTexture = std::make_unique<Texture>(getRenderer(), pixel, 1, 1, samplerInfo, false);
        getResourceManager<Texture>()->AddPersistent("builtin.normal", std::move(normalTexture));
    }
    {
        const uint8_t pixel[4] = {255, 127, 0, 255}; // AO, roughness, metallic
        gfx::SamplerInfo samplerInfo{};
        samplerInfo.minify = gfx::Filter::kNearest;
        samplerInfo.magnify = gfx::Filter::kNearest;
        samplerInfo.mipmap = gfx::Filter::kNearest;
        samplerInfo.anisotropic_filtering = false;
        auto mrTexture = std::make_unique<Texture>(getRenderer(), pixel, 1, 1, samplerInfo, false);
        getResourceManager<Texture>()->AddPersistent("builtin.mr", std::move(mrTexture));
    }

    /* default materials */
    {
        auto defaultMaterial = std::make_unique<Material>(getRenderer(), getResource<Shader>("builtin.fancy"));
        defaultMaterial->setAlbedoTexture(getResource<Texture>("builtin.white"));
        defaultMaterial->setNormalTexture(getResource<Texture>("builtin.normal"));
        defaultMaterial->setOcclusionRoughnessMetallicTexture(getResource<Texture>("builtin.mr"));
        getResourceManager<Material>()->AddPersistent("builtin.default", std::move(defaultMaterial));
    }
}

Application::~Application()
{
    m_renderer->GetDevice()->ShutdownImguiBackend();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext(im_gui_things.context);
}

void Application::gameLoop()
{
    LOG_DEBUG("Begin game loop...");

    auto lastTick = m_window->GetNanos();
    std::array<float, 20> delta_times{};

    struct DebugMenuState {
        bool menu_active = false;
        bool show_entity_boxes = false;
        bool show_bounding_volumes = false;
        bool enable_frame_limiter = false;
        bool triple_buffering = false;
        bool vsync = false;
        bool show_info_window = false;
    } debug_menu_state;
    debug_menu_state.enable_frame_limiter = m_configuration.enable_frame_limiter;
    switch (m_renderer->GetDevice()->GetPresentMode()) {
        case gfx::PresentMode::kDoubleBufferedNoVsync:
            debug_menu_state.triple_buffering = false;
            debug_menu_state.vsync = false;
            break;
        case gfx::PresentMode::kDoubleBufferedVsync:
            debug_menu_state.triple_buffering = false;
            debug_menu_state.vsync = true;
            break;
        case gfx::PresentMode::kTripleBuffered:
            debug_menu_state.triple_buffering = true;
            debug_menu_state.vsync = false;
    }

    int fps_limit = 240;
    auto beginFrame = std::chrono::steady_clock::now();
    auto endFrame = beginFrame + frametimeFromFPS(fps_limit);

    // single-threaded game loop
    while (m_window->IsRunning()) {
        /* logic */

        const float avg_fps = static_cast<float>(delta_times.size()) / std::accumulate(delta_times.begin(), delta_times.end(), 0.0f);

        Scene* scene = m_scene_manager->UpdateActiveScene(m_window->dt());

        uint64_t now = m_window->GetNanos();
        if (now - lastTick >= 1000000000LL * 5LL) [[unlikely]] {
            lastTick = now;
            LOG_DEBUG("fps: {}", std::lroundf(avg_fps));
            getRenderer()->GetDevice()->LogPerformanceInfo();
            m_window->ResetAvgFPS();
        }

        if (m_window->GetKeyPress(inputs::Key::K_F5)) {
            bool show_window = m_window->MouseCaptured();
            debug_menu_state.menu_active = show_window;
            m_window->SetRelativeMouseMode(!show_window);
        }

        if (m_window->GetKeyPress(inputs::Key::K_F6)) {
            debug_menu_state.show_info_window = !debug_menu_state.show_info_window;
        }

        if (m_window->GetKeyPress(inputs::Key::K_L)) {
            debug_menu_state.enable_frame_limiter ^= true;
        }

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // ImGui::ShowDemoWindow();

        // Stop mouse from moving the camera when the settings menu is open
        m_input_manager->SetDeviceActive(InputDevice::kMouse, !debug_menu_state.menu_active);

        if (debug_menu_state.menu_active) {
            if (ImGui::Begin("Settings", 0)) {
                ImGui::Text("FPS: %.3f", std::roundf(avg_fps));
                ImGui::Checkbox("Enable FPS limiter", &debug_menu_state.enable_frame_limiter);
                if (debug_menu_state.enable_frame_limiter) {
                    ImGui::SliderInt("FPS limit", &fps_limit, 10, 360);
                }
                if (debug_menu_state.triple_buffering) {
                    ImGui::BeginDisabled();
                }
                if (ImGui::Checkbox("Enable vsync", &debug_menu_state.vsync)) {
                    if (debug_menu_state.vsync) {
                        m_renderer->GetDevice()->ChangePresentMode(gfx::PresentMode::kDoubleBufferedVsync);
                    }
                    else {
                        m_renderer->GetDevice()->ChangePresentMode(gfx::PresentMode::kDoubleBufferedNoVsync);
                    }
                }
                if (debug_menu_state.triple_buffering) {
                    ImGui::EndDisabled();
                }
                if (ImGui::Checkbox("Triple buffering", &debug_menu_state.triple_buffering)) {
                    if (debug_menu_state.triple_buffering) {
                        debug_menu_state.vsync = false;
                        m_renderer->GetDevice()->ChangePresentMode(gfx::PresentMode::kTripleBuffered);
                    }
                    else {
                        if (debug_menu_state.vsync) {
                            m_renderer->GetDevice()->ChangePresentMode(gfx::PresentMode::kDoubleBufferedVsync);
                        }
                        else {
                            m_renderer->GetDevice()->ChangePresentMode(gfx::PresentMode::kDoubleBufferedNoVsync);
                        }
                    }
                }
                ImGui::Separator();
                ImGui::Checkbox("Show entity hitboxes", &debug_menu_state.show_entity_boxes);
                ImGui::Checkbox("Show bounding volumes", &debug_menu_state.show_bounding_volumes);
                ImGui::Separator();
                if (!scene) ImGui::BeginDisabled();
                // load gltf file dialog
                if (ImGui::Button("Load glTF")) {
                    std::filesystem::path path = util::OpenFileDialog({"glb"});
                    if (path.empty() == false) {
                        util::LoadGLTF(*scene, path.string(), false);
                    }
                }
                if (!scene) ImGui::EndDisabled();
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
                        // ImGui::Text("%.1f %.1f %.1f", t->position.x, t->position.y, t->position.z);
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
            if (debug_menu_state.show_entity_boxes) {
                if (CollisionSystem* colsys = scene->GetSystem<CollisionSystem>()) {
                    for (const auto& node : colsys->bvh_) {
                        if (node.type1 == CollisionSystem::BiTreeNode::Type::Entity) {
                            const glm::vec3 col =
                                (node.type1 == CollisionSystem::BiTreeNode::Type::BoundingVolume) ? glm::vec3{1.0f, 0.0f, 0.0f} : glm::vec3{0.0f, 1.0f, 0.0f};
                            DebugLine line1{glm::vec3{node.box1.min.x, node.box1.min.y, node.box1.min.z},
                                            glm::vec3{node.box1.max.x, node.box1.min.y, node.box1.min.z}, col};
                            debug_lines.push_back(line1);
                            DebugLine line2{glm::vec3{node.box1.min.x, node.box1.min.y, node.box1.min.z},
                                            glm::vec3{node.box1.min.x, node.box1.max.y, node.box1.min.z}, col};
                            debug_lines.push_back(line2);
                            DebugLine line3{glm::vec3{node.box1.max.x, node.box1.max.y, node.box1.min.z},
                                            glm::vec3{node.box1.max.x, node.box1.min.y, node.box1.min.z}, col};
                            debug_lines.push_back(line3);
                            DebugLine line4{glm::vec3{node.box1.max.x, node.box1.max.y, node.box1.min.z},
                                            glm::vec3{node.box1.min.x, node.box1.max.y, node.box1.min.z}, col};
                            debug_lines.push_back(line4);

                            DebugLine line5{glm::vec3{node.box1.min.x, node.box1.min.y, node.box1.min.z},
                                            glm::vec3{node.box1.min.x, node.box1.min.y, node.box1.max.z}, col};
                            debug_lines.push_back(line5);
                            DebugLine line6{glm::vec3{node.box1.min.x, node.box1.max.y, node.box1.min.z},
                                            glm::vec3{node.box1.min.x, node.box1.max.y, node.box1.max.z}, col};
                            debug_lines.push_back(line6);
                            DebugLine line7{glm::vec3{node.box1.max.x, node.box1.min.y, node.box1.min.z},
                                            glm::vec3{node.box1.max.x, node.box1.min.y, node.box1.max.z}, col};
                            debug_lines.push_back(line7);
                            DebugLine line8{glm::vec3{node.box1.max.x, node.box1.max.y, node.box1.min.z},
                                            glm::vec3{node.box1.max.x, node.box1.max.y, node.box1.max.z}, col};
                            debug_lines.push_back(line8);

                            DebugLine line9{glm::vec3{node.box1.min.x, node.box1.min.y, node.box1.max.z},
                                            glm::vec3{node.box1.max.x, node.box1.min.y, node.box1.max.z}, col};
                            debug_lines.push_back(line9);
                            DebugLine line10{glm::vec3{node.box1.min.x, node.box1.min.y, node.box1.max.z},
                                             glm::vec3{node.box1.min.x, node.box1.max.y, node.box1.max.z}, col};
                            debug_lines.push_back(line10);
                            DebugLine line11{glm::vec3{node.box1.max.x, node.box1.max.y, node.box1.max.z},
                                             glm::vec3{node.box1.max.x, node.box1.min.y, node.box1.max.z}, col};
                            debug_lines.push_back(line11);
                            DebugLine line12{glm::vec3{node.box1.max.x, node.box1.max.y, node.box1.max.z},
                                             glm::vec3{node.box1.min.x, node.box1.max.y, node.box1.max.z}, col};
                            debug_lines.push_back(line12);
                        }
                        if (node.type2 == CollisionSystem::BiTreeNode::Type::Entity) {
                            const glm::vec3 col =
                                (node.type2 == CollisionSystem::BiTreeNode::Type::BoundingVolume) ? glm::vec3{1.0f, 0.0f, 0.0f} : glm::vec3{0.0f, 1.0f, 0.0f};
                            DebugLine line1{glm::vec3{node.box2.min.x, node.box2.min.y, node.box2.min.z},
                                            glm::vec3{node.box2.max.x, node.box2.min.y, node.box2.min.z}, col};
                            debug_lines.push_back(line1);
                            DebugLine line2{glm::vec3{node.box2.min.x, node.box2.min.y, node.box2.min.z},
                                            glm::vec3{node.box2.min.x, node.box2.max.y, node.box2.min.z}, col};
                            debug_lines.push_back(line2);
                            DebugLine line3{glm::vec3{node.box2.max.x, node.box2.max.y, node.box2.min.z},
                                            glm::vec3{node.box2.max.x, node.box2.min.y, node.box2.min.z}, col};
                            debug_lines.push_back(line3);
                            DebugLine line4{glm::vec3{node.box2.max.x, node.box2.max.y, node.box2.min.z},
                                            glm::vec3{node.box2.min.x, node.box2.max.y, node.box2.min.z}, col};
                            debug_lines.push_back(line4);

                            DebugLine line5{glm::vec3{node.box2.min.x, node.box2.min.y, node.box2.min.z},
                                            glm::vec3{node.box2.min.x, node.box2.min.y, node.box2.max.z}, col};
                            debug_lines.push_back(line5);
                            DebugLine line6{glm::vec3{node.box2.min.x, node.box2.max.y, node.box2.min.z},
                                            glm::vec3{node.box2.min.x, node.box2.max.y, node.box2.max.z}, col};
                            debug_lines.push_back(line6);
                            DebugLine line7{glm::vec3{node.box2.max.x, node.box2.min.y, node.box2.min.z},
                                            glm::vec3{node.box2.max.x, node.box2.min.y, node.box2.max.z}, col};
                            debug_lines.push_back(line7);
                            DebugLine line8{glm::vec3{node.box2.max.x, node.box2.max.y, node.box2.min.z},
                                            glm::vec3{node.box2.max.x, node.box2.max.y, node.box2.max.z}, col};
                            debug_lines.push_back(line8);

                            DebugLine line9{glm::vec3{node.box2.min.x, node.box2.min.y, node.box2.max.z},
                                            glm::vec3{node.box2.max.x, node.box2.min.y, node.box2.max.z}, col};
                            debug_lines.push_back(line9);
                            DebugLine line10{glm::vec3{node.box2.min.x, node.box2.min.y, node.box2.max.z},
                                             glm::vec3{node.box2.min.x, node.box2.max.y, node.box2.max.z}, col};
                            debug_lines.push_back(line10);
                            DebugLine line11{glm::vec3{node.box2.max.x, node.box2.max.y, node.box2.max.z},
                                             glm::vec3{node.box2.max.x, node.box2.min.y, node.box2.max.z}, col};
                            debug_lines.push_back(line11);
                            DebugLine line12{glm::vec3{node.box2.max.x, node.box2.max.y, node.box2.max.z},
                                             glm::vec3{node.box2.min.x, node.box2.max.y, node.box2.max.z}, col};
                            debug_lines.push_back(line12);
                        }
                    }
                }
            }
            if (debug_menu_state.show_bounding_volumes) {
                if (CollisionSystem* colsys = scene->GetSystem<CollisionSystem>()) {
                    for (const auto& node : colsys->bvh_) {
                        if (node.type1 == CollisionSystem::BiTreeNode::Type::BoundingVolume) {
                            const glm::vec3 col =
                                (node.type1 == CollisionSystem::BiTreeNode::Type::BoundingVolume) ? glm::vec3{1.0f, 0.0f, 0.0f} : glm::vec3{0.0f, 1.0f, 0.0f};
                            DebugLine line1{glm::vec3{node.box1.min.x, node.box1.min.y, node.box1.min.z},
                                            glm::vec3{node.box1.max.x, node.box1.min.y, node.box1.min.z}, col};
                            debug_lines.push_back(line1);
                            DebugLine line2{glm::vec3{node.box1.min.x, node.box1.min.y, node.box1.min.z},
                                            glm::vec3{node.box1.min.x, node.box1.max.y, node.box1.min.z}, col};
                            debug_lines.push_back(line2);
                            DebugLine line3{glm::vec3{node.box1.max.x, node.box1.max.y, node.box1.min.z},
                                            glm::vec3{node.box1.max.x, node.box1.min.y, node.box1.min.z}, col};
                            debug_lines.push_back(line3);
                            DebugLine line4{glm::vec3{node.box1.max.x, node.box1.max.y, node.box1.min.z},
                                            glm::vec3{node.box1.min.x, node.box1.max.y, node.box1.min.z}, col};
                            debug_lines.push_back(line4);

                            DebugLine line5{glm::vec3{node.box1.min.x, node.box1.min.y, node.box1.min.z},
                                            glm::vec3{node.box1.min.x, node.box1.min.y, node.box1.max.z}, col};
                            debug_lines.push_back(line5);
                            DebugLine line6{glm::vec3{node.box1.min.x, node.box1.max.y, node.box1.min.z},
                                            glm::vec3{node.box1.min.x, node.box1.max.y, node.box1.max.z}, col};
                            debug_lines.push_back(line6);
                            DebugLine line7{glm::vec3{node.box1.max.x, node.box1.min.y, node.box1.min.z},
                                            glm::vec3{node.box1.max.x, node.box1.min.y, node.box1.max.z}, col};
                            debug_lines.push_back(line7);
                            DebugLine line8{glm::vec3{node.box1.max.x, node.box1.max.y, node.box1.min.z},
                                            glm::vec3{node.box1.max.x, node.box1.max.y, node.box1.max.z}, col};
                            debug_lines.push_back(line8);

                            DebugLine line9{glm::vec3{node.box1.min.x, node.box1.min.y, node.box1.max.z},
                                            glm::vec3{node.box1.max.x, node.box1.min.y, node.box1.max.z}, col};
                            debug_lines.push_back(line9);
                            DebugLine line10{glm::vec3{node.box1.min.x, node.box1.min.y, node.box1.max.z},
                                             glm::vec3{node.box1.min.x, node.box1.max.y, node.box1.max.z}, col};
                            debug_lines.push_back(line10);
                            DebugLine line11{glm::vec3{node.box1.max.x, node.box1.max.y, node.box1.max.z},
                                             glm::vec3{node.box1.max.x, node.box1.min.y, node.box1.max.z}, col};
                            debug_lines.push_back(line11);
                            DebugLine line12{glm::vec3{node.box1.max.x, node.box1.max.y, node.box1.max.z},
                                             glm::vec3{node.box1.min.x, node.box1.max.y, node.box1.max.z}, col};
                            debug_lines.push_back(line12);
                        }
                        if (node.type2 == CollisionSystem::BiTreeNode::Type::BoundingVolume) {
                            const glm::vec3 col =
                                (node.type2 == CollisionSystem::BiTreeNode::Type::BoundingVolume) ? glm::vec3{1.0f, 0.0f, 0.0f} : glm::vec3{0.0f, 1.0f, 0.0f};
                            DebugLine line1{glm::vec3{node.box2.min.x, node.box2.min.y, node.box2.min.z},
                                            glm::vec3{node.box2.max.x, node.box2.min.y, node.box2.min.z}, col};
                            debug_lines.push_back(line1);
                            DebugLine line2{glm::vec3{node.box2.min.x, node.box2.min.y, node.box2.min.z},
                                            glm::vec3{node.box2.min.x, node.box2.max.y, node.box2.min.z}, col};
                            debug_lines.push_back(line2);
                            DebugLine line3{glm::vec3{node.box2.max.x, node.box2.max.y, node.box2.min.z},
                                            glm::vec3{node.box2.max.x, node.box2.min.y, node.box2.min.z}, col};
                            debug_lines.push_back(line3);
                            DebugLine line4{glm::vec3{node.box2.max.x, node.box2.max.y, node.box2.min.z},
                                            glm::vec3{node.box2.min.x, node.box2.max.y, node.box2.min.z}, col};
                            debug_lines.push_back(line4);

                            DebugLine line5{glm::vec3{node.box2.min.x, node.box2.min.y, node.box2.min.z},
                                            glm::vec3{node.box2.min.x, node.box2.min.y, node.box2.max.z}, col};
                            debug_lines.push_back(line5);
                            DebugLine line6{glm::vec3{node.box2.min.x, node.box2.max.y, node.box2.min.z},
                                            glm::vec3{node.box2.min.x, node.box2.max.y, node.box2.max.z}, col};
                            debug_lines.push_back(line6);
                            DebugLine line7{glm::vec3{node.box2.max.x, node.box2.min.y, node.box2.min.z},
                                            glm::vec3{node.box2.max.x, node.box2.min.y, node.box2.max.z}, col};
                            debug_lines.push_back(line7);
                            DebugLine line8{glm::vec3{node.box2.max.x, node.box2.max.y, node.box2.min.z},
                                            glm::vec3{node.box2.max.x, node.box2.max.y, node.box2.max.z}, col};
                            debug_lines.push_back(line8);

                            DebugLine line9{glm::vec3{node.box2.min.x, node.box2.min.y, node.box2.max.z},
                                            glm::vec3{node.box2.max.x, node.box2.min.y, node.box2.max.z}, col};
                            debug_lines.push_back(line9);
                            DebugLine line10{glm::vec3{node.box2.min.x, node.box2.min.y, node.box2.max.z},
                                             glm::vec3{node.box2.min.x, node.box2.max.y, node.box2.max.z}, col};
                            debug_lines.push_back(line10);
                            DebugLine line11{glm::vec3{node.box2.max.x, node.box2.max.y, node.box2.max.z},
                                             glm::vec3{node.box2.max.x, node.box2.min.y, node.box2.max.z}, col};
                            debug_lines.push_back(line11);
                            DebugLine line12{glm::vec3{node.box2.max.x, node.box2.max.y, node.box2.max.z},
                                             glm::vec3{node.box2.min.x, node.box2.max.y, node.box2.max.z}, col};
                            debug_lines.push_back(line12);
                        }
                    }
                }
            }
            camera_transform = scene->GetComponent<TransformComponent>(scene->GetEntity("camera"))->world_matrix;
            auto mesh_render_system = scene->GetSystem<MeshRenderSystem>();
            static_list = mesh_render_system->GetStaticRenderList();
            dynamic_list = mesh_render_system->GetDynamicRenderList();
        }
        m_renderer->Render(getWindow()->GetWindowResized(), camera_transform, static_list, dynamic_list, debug_lines);
        debug_lines.clear(); // gets remade every frame :0

        /* poll events */
        m_window->GetInputAndEvents();

        /* fps limiter */
        if (m_configuration.enable_frame_limiter != debug_menu_state.enable_frame_limiter) {
            if (debug_menu_state.enable_frame_limiter) {
                m_configuration.enable_frame_limiter = true;
                // reset beginFrame and endFrame so the limiter doesn't hang for ages
                beginFrame = std::chrono::steady_clock::now();
                endFrame = beginFrame;
            }
            else {
                m_configuration.enable_frame_limiter = false;
            }
        }
        if (m_configuration.enable_frame_limiter) {
            std::this_thread::sleep_until(endFrame);
        }
        beginFrame = endFrame;
        endFrame = beginFrame + frametimeFromFPS(fps_limit);
        delta_times[m_window->GetFrameCount() % delta_times.size()] = m_window->dt();
    }

    m_renderer->GetDevice()->WaitIdle();
}

} // namespace engine
