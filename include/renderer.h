#ifndef ENGINE_INCLUDE_RENDERER_H_
#define ENGINE_INCLUDE_RENDERER_H_

#include <memory>
#include <unordered_map>

#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>

#include "gfx_device.h"
#include "systems/mesh_render_system.h"

namespace engine {

// A uniform struct that holds data of type T
template <typename T>
struct UniformDescriptor {
  const gfx::DescriptorSetLayout* layout;
  const gfx::DescriptorSet* set;
  struct UniformBufferData {
    T data;
  } uniform_buffer_data;
  gfx::UniformBuffer* uniform_buffer;
};

class Renderer {
 public:
  Renderer(const char* app_name, const char* app_version, SDL_Window* window,
           gfx::GraphicsSettings settings);

  ~Renderer();

  void PreRender(bool window_is_resized, glm::mat4 camera_transform);

  // staticList can be nullptr to render nothing
  void Render(const RenderList& static_list, const RenderList& dynamic_list);

  // getters

  GFXDevice* GetDevice() { return device_.get(); }

  const gfx::DescriptorSetLayout* GetGlobalSetLayout() {
    return global_uniform.layout;
  }

  const gfx::DescriptorSetLayout* GetFrameSetLayout() {
    return frame_uniform.layout;
  }

  const gfx::DescriptorSetLayout* GetMaterialSetLayout() {
    return material_set_layout;
  }

  std::unordered_map<gfx::SamplerInfo, const gfx::Sampler*> samplers;

 private:
  std::unique_ptr<GFXDevice> device_;

  struct CameraSettings {
    float vertical_fov_radians = glm::radians(70.0f);
    float clip_near = 0.5f;
    float clip_far = 1000.0f;
  } camera_settings_;

  // ALL vertex shaders must begin with:
  /*
  layout(set = 0, binding = 0) uniform GlobalSetUniformBuffer {
      mat4 proj;
  } globalSetUniformBuffer;

  layout(set = 1, binding = 0) uniform FrameSetUniformBuffer {
      mat4 view;
  } frameSetUniformBuffer;

  layout( push_constant ) uniform Constants {
      mat4 model;
  } constants;
  */
  // ALL fragment shaders must begin with:
  /*
  layout(set = 2, binding = 0) uniform sampler2D materialSetSampler;
  */

  // in vertex shader
  UniformDescriptor<glm::mat4> global_uniform;  // rarely updates; set 0
  UniformDescriptor<glm::mat4> frame_uniform;   // updates once per frame; set 1
  // in fragment shader
  const gfx::DescriptorSetLayout* material_set_layout;  // set 2

  float viewport_aspect_ratio_ = 1.0f;

  const gfx::Pipeline* last_bound_pipeline_ = nullptr;

  void DrawRenderList(gfx::DrawBuffer* draw_buffer, const RenderList& render_list);
};

}  // namespace engine

#endif
