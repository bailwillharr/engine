#ifndef ENGINE_INCLUDE_APPLICATION_H_
#define ENGINE_INCLUDE_APPLICATION_H_

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>

#include "gfx.h"
#include "input_manager.h"
#include "renderer.h"
#include "resource_manager.h"
#include "scene_manager.h"
#include "window.h"

namespace engine {

class Application {
   public:
    struct Configuration {
        bool enable_frame_limiter;
    };

    const char* const app_name;
    const char* const app_version;

    Application(const char* app_name, const char* app_version, gfx::GraphicsSettings graphics_settings, Configuration configuration);
    ~Application();
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    /* resource stuff */

    template <typename T>
    void RegisterResourceManager()
    {
        size_t hash = typeid(T).hash_code();
        assert(resource_managers_.contains(hash) == false && "Registering resource manager type more than once.");
        resource_managers_.emplace(hash, std::make_unique<ResourceManager<T>>());
    }

    template <typename T>
    std::shared_ptr<T> AddResource(const std::string& name, std::unique_ptr<T>&& resource)
    {
        auto resource_manager = GetResourceManager<T>();
        return resource_manager->Add(name, std::move(resource));
    }

    template <typename T>
    std::shared_ptr<T> GetResource(const std::string& name)
    {
        auto resource_manager = GetResourceManager<T>();
        return resource_manager->Get(name);
    }

    /* methods */
    void GameLoop();

    void SetFrameLimiter(bool on) { configuration_.enable_frame_limiter = on; }

    /* getters */
    Window* window() { return window_.get(); }
    InputManager* input_manager() { return input_manager_.get(); }
    SceneManager* scene_manager() { return scene_manager_.get(); }
    Renderer* renderer() { return renderer_.get(); }

    std::string GetResourcePath(const std::string relative_path) const { return (resources_path_ / relative_path).string(); }

    std::vector<Line> debug_lines{};

   private:
    std::unique_ptr<Window> window_;
    std::unique_ptr<InputManager> input_manager_;
    std::unique_ptr<Renderer> renderer_;
    std::unordered_map<size_t, std::unique_ptr<IResourceManager>> resource_managers_{};
    std::filesystem::path resources_path_;

    // Most resources in the game are owned by component-lists found in scenes in this scene manager:
    std::unique_ptr<SceneManager> scene_manager_;

    Configuration configuration_;

    template <typename T>
    ResourceManager<T>* GetResourceManager()
    {
        size_t hash = typeid(T).hash_code();
        auto it = resource_managers_.find(hash);
        if (it == resource_managers_.end()) {
            throw std::runtime_error("Cannot find resource manager.");
        }
        auto ptr = it->second.get();
        auto casted_ptr = dynamic_cast<ResourceManager<T>*>(ptr);
        assert(casted_ptr != nullptr);
        return casted_ptr;
    }
};

} // namespace engine

#endif