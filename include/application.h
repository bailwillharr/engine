#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>

#include "debug_line.h"
#include "gfx.h"
#include "resource_manager.h"

namespace engine {

class Window;       // forward-dec
class InputManager; // forward-dec
class Renderer;     // forward-dec
class SceneManager; // forward-dec

struct AppConfiguration {
    bool enable_frame_limiter;
};

class Application {
private:
    std::unique_ptr<Window> m_window;
    std::unique_ptr<InputManager> m_input_manager;
    std::unique_ptr<Renderer> m_renderer;
    std::unique_ptr<SceneManager> m_scene_manager;
    std::unordered_map<size_t, std::unique_ptr<IResourceManager>> m_resource_managers{};
    std::filesystem::path m_resources_path;
    AppConfiguration m_configuration;

public:
    const char* const app_name;
    const char* const app_version;
    std::vector<DebugLine> debug_lines{};

public:
    Application(const char* app_name, const char* app_version, gfx::GraphicsSettings graphics_settings, AppConfiguration configuration);
    Application(const Application&) = delete;

    ~Application();

    Application& operator=(const Application&) = delete;

    template <typename T>
    void registerResourceManager();
    template <typename T>
    std::shared_ptr<T> addResource(const std::string& name, std::unique_ptr<T>&& resource);
    template <typename T>
    std::shared_ptr<T> getResource(const std::string& name);
    void gameLoop();
    void setFrameLimiter(bool on) { m_configuration.enable_frame_limiter = on; }
    Window* getWindow() { return m_window.get(); }
    InputManager* getInputManager() { return m_input_manager.get(); }
    SceneManager* getSceneManager() { return m_scene_manager.get(); }
    Renderer* getRenderer() { return m_renderer.get(); }
    std::string getResourcePath(const std::string relative_path) const { return (m_resources_path / relative_path).string(); }

private:
    template <typename T>
    ResourceManager<T>* getResourceManager();
};

template <typename T>
void Application::registerResourceManager()
{
    size_t hash = typeid(T).hash_code();
    assert(m_resource_managers.contains(hash) == false && "Registering resource manager type more than once.");
    m_resource_managers.emplace(hash, std::make_unique<ResourceManager<T>>());
}

template <typename T>
std::shared_ptr<T> Application::addResource(const std::string& name, std::unique_ptr<T>&& resource)
{
    auto resource_manager = getResourceManager<T>();
    return resource_manager->Add(name, std::move(resource));
}

template <typename T>
std::shared_ptr<T> Application::getResource(const std::string& name)
{
    auto resource_manager = getResourceManager<T>();
    return resource_manager->Get(name);
}

template <typename T>
ResourceManager<T>* Application::getResourceManager()
{
    size_t hash = typeid(T).hash_code();
    auto it = m_resource_managers.find(hash);
    if (it == m_resource_managers.end()) {
        throw std::runtime_error("Cannot find resource manager.");
    }
    auto ptr = it->second.get();
    auto casted_ptr = dynamic_cast<ResourceManager<T>*>(ptr);
    assert(casted_ptr != nullptr);
    return casted_ptr;
}

} // namespace engine