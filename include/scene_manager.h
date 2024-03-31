#pragma once

#include <memory>
#include <vector>

#include "resource_manager.h"
#include "scene.h"

namespace engine {

class Application;

class SceneManager {
   public:
    SceneManager(Application* app);
    ~SceneManager();
    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;

    // creates an empty scene and sets it as active
    Scene* CreateEmptyScene();

    // nullptr deactivates the active scene
    void SetActiveScene(Scene* scene)
    {
        if (scene == nullptr) {
            active_scene_index_ = -1;
        }
        else {
            // linear search for scene
            int index = 0;
            for (const auto& unique_scene : scenes_) {
                if (unique_scene.get() == scene) {
                    break;
                }
                ++index;
            }
            if (static_cast<size_t>(index) >= scenes_.size()) {
                throw std::runtime_error("Failed to find active scene!");
            }
            else {
                active_scene_index_ = index;
            }
        }
    }

    // returns active scene, nullptr if no scene active
    Scene* UpdateActiveScene(float ts);
    Scene* GetActiveScene() { return scenes_.at(active_scene_index_).get(); }

   private:
    Application* const app_;

    std::vector<std::unique_ptr<Scene>> scenes_;
    int active_scene_index_ = -1;
};

} // namespace engine