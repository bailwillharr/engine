#ifndef ENGINE_INCLUDE_SCENE_MANAGER_H_
#define ENGINE_INCLUDE_SCENE_MANAGER_H_

#include <memory>
#include <vector>

#include "resource_manager.hpp"
#include "scene.hpp"

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

  void UpdateActiveScene(float ts);

 private:
  Application* const app_;

  std::vector<std::unique_ptr<Scene>> scenes_;
  int active_scene_index_ = -1;
};

}  // namespace engine

#endif