#include "scene_manager.h"

#include "scene.h"
#include "resource_texture.h"

#include <assert.h>

namespace engine {

SceneManager::SceneManager(Application* app) : app_(app) {}

SceneManager::~SceneManager() {}

Scene* SceneManager::CreateEmptyScene()
{
    auto scene = std::make_unique<Scene>(app_);
    scenes_.emplace_back(std::move(scene));
    return scenes_.back().get();
}

Scene* SceneManager::UpdateActiveScene(float ts)
{
    if (active_scene_index_ >= 0) [[likely]] {
        assert((size_t)active_scene_index_ < scenes_.size());
        Scene* activeScene = scenes_[active_scene_index_].get();
        activeScene->Update(ts);
        return activeScene;
    }
    else {
        return nullptr;
    }
}

} // namespace engine
