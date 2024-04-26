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
#include "components/collider.h"
#include "scene.h"
#include "scene_manager.h"
#include "window.h"

#ifdef _MSC_VER
#include <windows.h>
#include <direct.h>
#define WIN_MAX_PATH 260
#endif
#include <systems/collisions.h>

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

#ifdef _WIN32
static std::string openGLTFDialog() {
    OPENFILENAMEA ofn;       // common dialog box structure
    CHAR szFile[260] = { 0 };       // if using TCHAR macros, use TCHAR array

    // Initialize OPENFILENAME
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.lpstrFile[0] = '\0';
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "GLTF Files (*.gltf;*.glb)\0*.gltf;*.glb\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    // Display the Open dialog box
    if (GetOpenFileNameA(&ofn) == TRUE)
    {
        return ofn.lpstrFile;
    }
    else
    {
        return ""; // User cancelled the dialog
    }
}
#endif

static auto frametimeFromFPS(int fps) { return std::chrono::nanoseconds(1'000'000'000 / fps); }

Application::Application(const char* appName, const char* appVersion, gfx::GraphicsSettings graphicsSettings, Configuration configuration)
    : app_name(appName), app_version(appVersion), configuration_(configuration)
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

    renderer_ = std::make_unique<Renderer>(*this, graphicsSettings);

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
        auto fancyShader =
            std::make_unique<Shader>(renderer(), GetResourcePath("engine/shaders/fancy.vert"), GetResourcePath("engine/shaders/fancy.frag"), shaderSettings);
        GetResourceManager<Shader>()->AddPersistent("builtin.fancy", std::move(fancyShader));
    }

    /* default textures */
    {
        const uint8_t pixel[4] = {255, 255, 255, 255};
        gfx::SamplerInfo samplerInfo{};
        samplerInfo.minify = gfx::Filter::kNearest;
        samplerInfo.magnify = gfx::Filter::kNearest;
        samplerInfo.mipmap = gfx::Filter::kNearest;
        samplerInfo.anisotropic_filtering = false;
        auto whiteTexture = std::make_unique<Texture>(renderer(), pixel, 1, 1, samplerInfo, true);
        GetResourceManager<Texture>()->AddPersistent("builtin.white", std::move(whiteTexture));
    }
    {
        const uint8_t pixel[4] = {0, 0, 0, 255};
        gfx::SamplerInfo samplerInfo{};
        samplerInfo.minify = gfx::Filter::kNearest;
        samplerInfo.magnify = gfx::Filter::kNearest;
        samplerInfo.mipmap = gfx::Filter::kNearest;
        samplerInfo.anisotropic_filtering = false;
        auto blackTexture = std::make_unique<Texture>(renderer(), pixel, 1, 1, samplerInfo, true);
        GetResourceManager<Texture>()->AddPersistent("builtin.black", std::move(blackTexture));
    }
    {
        const uint8_t pixel[4] = {127, 127, 255, 255};
        gfx::SamplerInfo samplerInfo{};
        samplerInfo.minify = gfx::Filter::kNearest;
        samplerInfo.magnify = gfx::Filter::kNearest;
        samplerInfo.mipmap = gfx::Filter::kNearest;
        samplerInfo.anisotropic_filtering = false;
        auto normalTexture = std::make_unique<Texture>(renderer(), pixel, 1, 1, samplerInfo, false);
        GetResourceManager<Texture>()->AddPersistent("builtin.normal", std::move(normalTexture));
    }
    {
        const uint8_t pixel[4] = {255, 127, 0, 255}; // AO, roughness, metallic
        gfx::SamplerInfo samplerInfo{};
        samplerInfo.minify = gfx::Filter::kNearest;
        samplerInfo.magnify = gfx::Filter::kNearest;
        samplerInfo.mipmap = gfx::Filter::kNearest;
        samplerInfo.anisotropic_filtering = false;
        auto mrTexture = std::make_unique<Texture>(renderer(), pixel, 1, 1, samplerInfo, false);
        GetResourceManager<Texture>()->AddPersistent("builtin.mr", std::move(mrTexture));
    }

    /* default materials */
    {
        auto defaultMaterial = std::make_unique<Material>(renderer(), GetResource<Shader>("builtin.fancy"));
        defaultMaterial->SetAlbedoTexture(GetResource<Texture>("builtin.white"));
        defaultMaterial->SetNormalTexture(GetResource<Texture>("builtin.normal"));
        defaultMaterial->SetOcclusionRoughnessMetallicTexture(GetResource<Texture>("builtin.mr"));
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

    auto lastTick = window_->GetNanos();
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
    debug_menu_state.enable_frame_limiter = configuration_.enable_frame_limiter;
    switch (renderer_->GetDevice()->GetPresentMode()) {
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
    while (window_->IsRunning()) {
        /* logic */

        const float avg_fps = static_cast<float>(delta_times.size()) / std::accumulate(delta_times.begin(), delta_times.end(), 0.0f);

        Scene* scene = scene_manager_->UpdateActiveScene(window_->dt());

        uint64_t now = window_->GetNanos();
        if (now - lastTick >= 1000000000LL * 5LL) [[unlikely]] {
            lastTick = now;
            LOG_DEBUG("fps: {}", std::lroundf(avg_fps));
            renderer()->GetDevice()->LogPerformanceInfo();
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

        if (window_->GetKeyPress(inputs::Key::K_L)) {
            debug_menu_state.enable_frame_limiter ^= true;
        }

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        //ImGui::ShowDemoWindow();

        // Stop mouse from moving the camera when the settings menu is open
        input_manager_->SetDeviceActive(InputDevice::kMouse, !debug_menu_state.menu_active);

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
                        renderer_->GetDevice()->ChangePresentMode(gfx::PresentMode::kDoubleBufferedVsync);
                    }
                    else {
                        renderer_->GetDevice()->ChangePresentMode(gfx::PresentMode::kDoubleBufferedNoVsync);
                    }
                }
                if (debug_menu_state.triple_buffering) {
                    ImGui::EndDisabled();
                }
                if (ImGui::Checkbox("Triple buffering", &debug_menu_state.triple_buffering)) {
                    if (debug_menu_state.triple_buffering) {
                        debug_menu_state.vsync = false;
                        renderer_->GetDevice()->ChangePresentMode(gfx::PresentMode::kTripleBuffered);
                    }
                    else {
                        if (debug_menu_state.vsync) {
                            renderer_->GetDevice()->ChangePresentMode(gfx::PresentMode::kDoubleBufferedVsync);
                        }
                        else {
                            renderer_->GetDevice()->ChangePresentMode(gfx::PresentMode::kDoubleBufferedNoVsync);
                        }
                    }
                }
                ImGui::Separator();
                ImGui::Checkbox("Show entity hitboxes", &debug_menu_state.show_entity_boxes);
                ImGui::Checkbox("Show bounding volumes", &debug_menu_state.show_bounding_volumes);
                ImGui::Separator();
#ifndef _WIN32
                ImGui::BeginDisabled();
#endif
                // load gltf file dialog
                if (ImGui::Button("Load glTF")) {
#ifdef _WIN32
                    std::string path = std::filesystem::path(openGLTFDialog()).string();

#endif
                }
#ifndef _WIN32
                ImGui::EndDisabled();
#endif
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
                            Line line1{glm::vec3{node.box1.min.x, node.box1.min.y, node.box1.min.z},
                                       glm::vec3{node.box1.max.x, node.box1.min.y, node.box1.min.z}, col};
                            debug_lines.push_back(line1);
                            Line line2{glm::vec3{node.box1.min.x, node.box1.min.y, node.box1.min.z},
                                       glm::vec3{node.box1.min.x, node.box1.max.y, node.box1.min.z}, col};
                            debug_lines.push_back(line2);
                            Line line3{glm::vec3{node.box1.max.x, node.box1.max.y, node.box1.min.z},
                                       glm::vec3{node.box1.max.x, node.box1.min.y, node.box1.min.z}, col};
                            debug_lines.push_back(line3);
                            Line line4{glm::vec3{node.box1.max.x, node.box1.max.y, node.box1.min.z},
                                       glm::vec3{node.box1.min.x, node.box1.max.y, node.box1.min.z}, col};
                            debug_lines.push_back(line4);

                            Line line5{glm::vec3{node.box1.min.x, node.box1.min.y, node.box1.min.z},
                                       glm::vec3{node.box1.min.x, node.box1.min.y, node.box1.max.z}, col};
                            debug_lines.push_back(line5);
                            Line line6{glm::vec3{node.box1.min.x, node.box1.max.y, node.box1.min.z},
                                       glm::vec3{node.box1.min.x, node.box1.max.y, node.box1.max.z}, col};
                            debug_lines.push_back(line6);
                            Line line7{glm::vec3{node.box1.max.x, node.box1.min.y, node.box1.min.z},
                                       glm::vec3{node.box1.max.x, node.box1.min.y, node.box1.max.z}, col};
                            debug_lines.push_back(line7);
                            Line line8{glm::vec3{node.box1.max.x, node.box1.max.y, node.box1.min.z},
                                       glm::vec3{node.box1.max.x, node.box1.max.y, node.box1.max.z}, col};
                            debug_lines.push_back(line8);

                            Line line9{glm::vec3{node.box1.min.x, node.box1.min.y, node.box1.max.z},
                                       glm::vec3{node.box1.max.x, node.box1.min.y, node.box1.max.z}, col};
                            debug_lines.push_back(line9);
                            Line line10{glm::vec3{node.box1.min.x, node.box1.min.y, node.box1.max.z},
                                        glm::vec3{node.box1.min.x, node.box1.max.y, node.box1.max.z}, col};
                            debug_lines.push_back(line10);
                            Line line11{glm::vec3{node.box1.max.x, node.box1.max.y, node.box1.max.z},
                                        glm::vec3{node.box1.max.x, node.box1.min.y, node.box1.max.z}, col};
                            debug_lines.push_back(line11);
                            Line line12{glm::vec3{node.box1.max.x, node.box1.max.y, node.box1.max.z},
                                        glm::vec3{node.box1.min.x, node.box1.max.y, node.box1.max.z}, col};
                            debug_lines.push_back(line12);
                        }
                        if (node.type2 == CollisionSystem::BiTreeNode::Type::Entity) {
                            const glm::vec3 col =
                                (node.type2 == CollisionSystem::BiTreeNode::Type::BoundingVolume) ? glm::vec3{1.0f, 0.0f, 0.0f} : glm::vec3{0.0f, 1.0f, 0.0f};
                            Line line1{glm::vec3{node.box2.min.x, node.box2.min.y, node.box2.min.z},
                                       glm::vec3{node.box2.max.x, node.box2.min.y, node.box2.min.z}, col};
                            debug_lines.push_back(line1);
                            Line line2{glm::vec3{node.box2.min.x, node.box2.min.y, node.box2.min.z},
                                       glm::vec3{node.box2.min.x, node.box2.max.y, node.box2.min.z}, col};
                            debug_lines.push_back(line2);
                            Line line3{glm::vec3{node.box2.max.x, node.box2.max.y, node.box2.min.z},
                                       glm::vec3{node.box2.max.x, node.box2.min.y, node.box2.min.z}, col};
                            debug_lines.push_back(line3);
                            Line line4{glm::vec3{node.box2.max.x, node.box2.max.y, node.box2.min.z},
                                       glm::vec3{node.box2.min.x, node.box2.max.y, node.box2.min.z}, col};
                            debug_lines.push_back(line4);

                            Line line5{glm::vec3{node.box2.min.x, node.box2.min.y, node.box2.min.z},
                                       glm::vec3{node.box2.min.x, node.box2.min.y, node.box2.max.z}, col};
                            debug_lines.push_back(line5);
                            Line line6{glm::vec3{node.box2.min.x, node.box2.max.y, node.box2.min.z},
                                       glm::vec3{node.box2.min.x, node.box2.max.y, node.box2.max.z}, col};
                            debug_lines.push_back(line6);
                            Line line7{glm::vec3{node.box2.max.x, node.box2.min.y, node.box2.min.z},
                                       glm::vec3{node.box2.max.x, node.box2.min.y, node.box2.max.z}, col};
                            debug_lines.push_back(line7);
                            Line line8{glm::vec3{node.box2.max.x, node.box2.max.y, node.box2.min.z},
                                       glm::vec3{node.box2.max.x, node.box2.max.y, node.box2.max.z}, col};
                            debug_lines.push_back(line8);

                            Line line9{glm::vec3{node.box2.min.x, node.box2.min.y, node.box2.max.z},
                                       glm::vec3{node.box2.max.x, node.box2.min.y, node.box2.max.z}, col};
                            debug_lines.push_back(line9);
                            Line line10{glm::vec3{node.box2.min.x, node.box2.min.y, node.box2.max.z},
                                        glm::vec3{node.box2.min.x, node.box2.max.y, node.box2.max.z}, col};
                            debug_lines.push_back(line10);
                            Line line11{glm::vec3{node.box2.max.x, node.box2.max.y, node.box2.max.z},
                                        glm::vec3{node.box2.max.x, node.box2.min.y, node.box2.max.z}, col};
                            debug_lines.push_back(line11);
                            Line line12{glm::vec3{node.box2.max.x, node.box2.max.y, node.box2.max.z},
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
                            Line line1{glm::vec3{node.box1.min.x, node.box1.min.y, node.box1.min.z},
                                       glm::vec3{node.box1.max.x, node.box1.min.y, node.box1.min.z}, col};
                            debug_lines.push_back(line1);
                            Line line2{glm::vec3{node.box1.min.x, node.box1.min.y, node.box1.min.z},
                                       glm::vec3{node.box1.min.x, node.box1.max.y, node.box1.min.z}, col};
                            debug_lines.push_back(line2);
                            Line line3{glm::vec3{node.box1.max.x, node.box1.max.y, node.box1.min.z},
                                       glm::vec3{node.box1.max.x, node.box1.min.y, node.box1.min.z}, col};
                            debug_lines.push_back(line3);
                            Line line4{glm::vec3{node.box1.max.x, node.box1.max.y, node.box1.min.z},
                                       glm::vec3{node.box1.min.x, node.box1.max.y, node.box1.min.z}, col};
                            debug_lines.push_back(line4);

                            Line line5{glm::vec3{node.box1.min.x, node.box1.min.y, node.box1.min.z},
                                       glm::vec3{node.box1.min.x, node.box1.min.y, node.box1.max.z}, col};
                            debug_lines.push_back(line5);
                            Line line6{glm::vec3{node.box1.min.x, node.box1.max.y, node.box1.min.z},
                                       glm::vec3{node.box1.min.x, node.box1.max.y, node.box1.max.z}, col};
                            debug_lines.push_back(line6);
                            Line line7{glm::vec3{node.box1.max.x, node.box1.min.y, node.box1.min.z},
                                       glm::vec3{node.box1.max.x, node.box1.min.y, node.box1.max.z}, col};
                            debug_lines.push_back(line7);
                            Line line8{glm::vec3{node.box1.max.x, node.box1.max.y, node.box1.min.z},
                                       glm::vec3{node.box1.max.x, node.box1.max.y, node.box1.max.z}, col};
                            debug_lines.push_back(line8);

                            Line line9{glm::vec3{node.box1.min.x, node.box1.min.y, node.box1.max.z},
                                       glm::vec3{node.box1.max.x, node.box1.min.y, node.box1.max.z}, col};
                            debug_lines.push_back(line9);
                            Line line10{glm::vec3{node.box1.min.x, node.box1.min.y, node.box1.max.z},
                                        glm::vec3{node.box1.min.x, node.box1.max.y, node.box1.max.z}, col};
                            debug_lines.push_back(line10);
                            Line line11{glm::vec3{node.box1.max.x, node.box1.max.y, node.box1.max.z},
                                        glm::vec3{node.box1.max.x, node.box1.min.y, node.box1.max.z}, col};
                            debug_lines.push_back(line11);
                            Line line12{glm::vec3{node.box1.max.x, node.box1.max.y, node.box1.max.z},
                                        glm::vec3{node.box1.min.x, node.box1.max.y, node.box1.max.z}, col};
                            debug_lines.push_back(line12);
                        }
                        if (node.type2 == CollisionSystem::BiTreeNode::Type::BoundingVolume) {
                            const glm::vec3 col =
                                (node.type2 == CollisionSystem::BiTreeNode::Type::BoundingVolume) ? glm::vec3{1.0f, 0.0f, 0.0f} : glm::vec3{0.0f, 1.0f, 0.0f};
                            Line line1{glm::vec3{node.box2.min.x, node.box2.min.y, node.box2.min.z},
                                       glm::vec3{node.box2.max.x, node.box2.min.y, node.box2.min.z}, col};
                            debug_lines.push_back(line1);
                            Line line2{glm::vec3{node.box2.min.x, node.box2.min.y, node.box2.min.z},
                                       glm::vec3{node.box2.min.x, node.box2.max.y, node.box2.min.z}, col};
                            debug_lines.push_back(line2);
                            Line line3{glm::vec3{node.box2.max.x, node.box2.max.y, node.box2.min.z},
                                       glm::vec3{node.box2.max.x, node.box2.min.y, node.box2.min.z}, col};
                            debug_lines.push_back(line3);
                            Line line4{glm::vec3{node.box2.max.x, node.box2.max.y, node.box2.min.z},
                                       glm::vec3{node.box2.min.x, node.box2.max.y, node.box2.min.z}, col};
                            debug_lines.push_back(line4);

                            Line line5{glm::vec3{node.box2.min.x, node.box2.min.y, node.box2.min.z},
                                       glm::vec3{node.box2.min.x, node.box2.min.y, node.box2.max.z}, col};
                            debug_lines.push_back(line5);
                            Line line6{glm::vec3{node.box2.min.x, node.box2.max.y, node.box2.min.z},
                                       glm::vec3{node.box2.min.x, node.box2.max.y, node.box2.max.z}, col};
                            debug_lines.push_back(line6);
                            Line line7{glm::vec3{node.box2.max.x, node.box2.min.y, node.box2.min.z},
                                       glm::vec3{node.box2.max.x, node.box2.min.y, node.box2.max.z}, col};
                            debug_lines.push_back(line7);
                            Line line8{glm::vec3{node.box2.max.x, node.box2.max.y, node.box2.min.z},
                                       glm::vec3{node.box2.max.x, node.box2.max.y, node.box2.max.z}, col};
                            debug_lines.push_back(line8);

                            Line line9{glm::vec3{node.box2.min.x, node.box2.min.y, node.box2.max.z},
                                       glm::vec3{node.box2.max.x, node.box2.min.y, node.box2.max.z}, col};
                            debug_lines.push_back(line9);
                            Line line10{glm::vec3{node.box2.min.x, node.box2.min.y, node.box2.max.z},
                                        glm::vec3{node.box2.min.x, node.box2.max.y, node.box2.max.z}, col};
                            debug_lines.push_back(line10);
                            Line line11{glm::vec3{node.box2.max.x, node.box2.max.y, node.box2.max.z},
                                        glm::vec3{node.box2.max.x, node.box2.min.y, node.box2.max.z}, col};
                            debug_lines.push_back(line11);
                            Line line12{glm::vec3{node.box2.max.x, node.box2.max.y, node.box2.max.z},
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
        renderer_->Render(window()->GetWindowResized(), camera_transform, static_list, dynamic_list, debug_lines);
        debug_lines.clear(); // gets remade every frame :0

        /* poll events */
        window_->GetInputAndEvents();

        /* fps limiter */
        if (configuration_.enable_frame_limiter != debug_menu_state.enable_frame_limiter) {
            if (debug_menu_state.enable_frame_limiter) {
                configuration_.enable_frame_limiter = true;
                // reset beginFrame and endFrame so the limiter doesn't hang for ages
                beginFrame = std::chrono::steady_clock::now();
                endFrame = beginFrame;
            }
            else {
                configuration_.enable_frame_limiter = false;
            }
        }
        if (configuration_.enable_frame_limiter) {
            std::this_thread::sleep_until(endFrame);
        }
        beginFrame = endFrame;
        endFrame = beginFrame + frametimeFromFPS(fps_limit);
        delta_times[window_->GetFrameCount() % delta_times.size()] = window_->dt();
    }

    renderer_->GetDevice()->WaitIdle();
}

} // namespace engine
