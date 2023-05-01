#include "scene_manager.h"

#include "scene.h"
#include "resources/texture.h"

#include <assert.h>

namespace engine {

	SceneManager::SceneManager(Application* app)
		: app_(app)
	{
	}

	SceneManager::~SceneManager() {}

	Scene* SceneManager::CreateEmptyScene()
	{
		auto scene = std::make_unique<Scene>(app_);
		scenes_.emplace_back(std::move(scene));
		active_scene_index_ = (int)scenes_.size() - 1;
		return scenes_.back().get();
	}

	void SceneManager::UpdateActiveScene(float ts)
	{
		if (active_scene_index_ >= 0) [[likely]] {
			assert((size_t)active_scene_index_ < scenes_.size());
			scenes_[active_scene_index_]->Update(ts);
		}
	}

}
