#ifndef ENGINE_INCLUDE_APPLICATION_H_
#define ENGINE_INCLUDE_APPLICATION_H_

#include <assert.h>

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>

#include <glm/mat4x4.hpp>

#include "gfx.h"
#include "gfx_device.h"
#include "input_manager.h"
#include "resource_manager.h"
#include "scene_manager.h"
#include "window.h"

namespace engine {

struct RenderData {
  std::unique_ptr<GFXDevice> gfxdev;
  gfx::DrawBuffer* draw_buffer = nullptr;

  /* uniforms for engine globals */
  const gfx::DescriptorSetLayout* global_set_layout;
  const gfx::DescriptorSet* global_set;
  struct GlobalSetUniformBuffer {
    glm::mat4 proj;
  };
  gfx::UniformBuffer* global_set_uniform_buffer;

  /* uniforms for per-frame data */
  const gfx::DescriptorSetLayout* frame_set_layout;
  const gfx::DescriptorSet* frame_set;
  struct FrameSetUniformBuffer {
    glm::mat4 view;
  };
  gfx::UniformBuffer* frame_set_uniform_buffer;

  /* this descriptor set is bound per-material */
  const gfx::DescriptorSetLayout* material_set_layout;
  std::unordered_map<gfx::SamplerInfo, const gfx::Sampler*> samplers{};
};

class Application {
 public:
  Application(const char* app_name, const char* app_version,
              gfx::GraphicsSettings graphics_settings);
  ~Application();
  Application(const Application&) = delete;
  Application& operator=(const Application&) = delete;

  /* resource stuff */

  template <typename T>
  void RegisterResourceManager() {
    size_t hash = typeid(T).hash_code();
    assert(resource_managers_.contains(hash) == false &&
           "Registering resource manager type more than once.");
    resource_managers_.emplace(hash, std::make_unique<ResourceManager<T>>());
  }

  template <typename T>
  std::shared_ptr<T> AddResource(const std::string& name,
                                 std::unique_ptr<T>&& resource) {
    auto resource_manager = GetResourceManager<T>();
    return resource_manager->Add(name, std::move(resource));
  }

  template <typename T>
  std::shared_ptr<T> GetResource(const std::string& name) {
    auto resource_manager = GetResourceManager<T>();
    return resource_manager->Get(name);
  }

  /* methods */
  void GameLoop();

  void SetFrameLimiter(bool on) { enable_frame_limiter_ = on; }

  /* getters */
  Window* window() { return window_.get(); }
  GFXDevice* gfxdev() { return render_data_.gfxdev.get(); }
  InputManager* input_manager() { return input_manager_.get(); }
  SceneManager* scene_manager() { return scene_manager_.get(); }

  std::string GetResourcePath(const std::string relative_path) {
    return (resources_path_ / relative_path).string();
  }

  RenderData render_data_{};

 private:
  std::unique_ptr<Window> window_;
  std::unique_ptr<InputManager> input_manager_;
  std::unique_ptr<SceneManager> scene_manager_;
  std::unordered_map<size_t, std::unique_ptr<IResourceManager>>
      resource_managers_{};
  std::filesystem::path resources_path_;

  bool enable_frame_limiter_ = true;

  template <typename T>
  ResourceManager<T>* GetResourceManager() {
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

}  // namespace engine

#endif