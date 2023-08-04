#include "systems/render.h"

#include "application.h"
#include "window.h"
#include "gfx_device.h"

#include "resources/material.h"
#include "resources/shader.h"
#include "resources/texture.h"

#include "resources/mesh.h"

#include <glm/mat4x4.hpp>

namespace engine {

RenderSystem::RenderSystem(Scene* scene)
    : System(scene, {typeid(TransformComponent).hash_code(),
                     typeid(RenderableComponent).hash_code()}),
      gfx_(scene_->app()->gfxdev()) {}

RenderSystem::~RenderSystem() {}

void RenderSystem::OnUpdate(float ts) {
  (void)ts;

  RenderData& render_data = scene_->app()->render_data_;

  /* camera stuff */
  const auto camera_transform =
      scene_->GetComponent<TransformComponent>(camera_.cam_entity);

  // do not render if camera is not set
  if (camera_transform == nullptr) return;

  if (scene_->app()->window()->GetWindowResized()) {
    uint32_t w, h;
    gfx_->GetViewportSize(&w, &h);
    viewport_aspect_ratio_ = (float)w / (float)h;
    const float vertical_fov_radians =
        glm::radians(camera_.vertical_fov_degrees);
    const glm::mat4 proj_matrix =
        glm::perspectiveZO(vertical_fov_radians, viewport_aspect_ratio_,
                           camera_.clip_near, camera_.clip_far);
    /* update SET 0 (rarely changing uniforms)*/
    RenderData::GlobalSetUniformBuffer global_set_uniform_buffer{};
    global_set_uniform_buffer.proj = proj_matrix;
    gfx_->WriteUniformBuffer(render_data.global_set_uniform_buffer, 0,
                             sizeof(RenderData::GlobalSetUniformBuffer),
                             &global_set_uniform_buffer);
  }

  glm::mat4 view_matrix = glm::inverse(camera_transform->world_matrix);
  /* update SET 1 (per frame uniforms) */
  RenderData::FrameSetUniformBuffer frame_set_uniform_buffer{};
  frame_set_uniform_buffer.view = view_matrix;
  gfx_->WriteUniformBuffer(render_data.frame_set_uniform_buffer, 0,
                           sizeof(RenderData::FrameSetUniformBuffer),
                           &frame_set_uniform_buffer);

  /* render all renderable entities */

  struct PushConstants {
    glm::mat4 model;
  };

  struct DrawCallData {
    const gfx::Buffer* vb;
    const gfx::Buffer* ib;
    const gfx::DescriptorSet* material_set;
    uint32_t index_count;
    PushConstants push_constants;
  };
  std::vector<std::tuple<int, const gfx::Pipeline*, DrawCallData>>
      unsorted_draw_calls{};

  for (uint32_t entity : entities_) {
    auto r = scene_->GetComponent<RenderableComponent>(entity);
    assert(r != nullptr);
    assert(r->material != nullptr);
    assert(r->material->texture_ != nullptr);
    if (r->shown == false) continue;

    auto t = scene_->GetComponent<TransformComponent>(entity);
    assert(t != nullptr);

    int render_order = r->material->GetShader()->GetRenderOrder();

    const gfx::Pipeline* pipeline = r->material->GetShader()->GetPipeline();

    DrawCallData data{};
    if (r->mesh) {
      data.vb = r->mesh->GetVB();
      data.ib = r->mesh->GetIB();
    }
    data.material_set = r->material->texture_->GetDescriptorSet();
    if (r->index_count_override == 0) {
      assert(r->mesh != nullptr);
      data.index_count = r->mesh->GetCount();
    } else {
      data.index_count = r->index_count_override;
    }
    data.push_constants.model = t->world_matrix;

    unsorted_draw_calls.emplace_back(
        std::make_tuple(render_order, pipeline, data));
  }

  std::vector<std::pair<const gfx::Pipeline*, DrawCallData>> draw_calls{};
  draw_calls.reserve(unsorted_draw_calls.size());

  /* sort the draw calls */
  for (int i = 0; i <= resources::Shader::kHighestRenderOrder; i++) {
    for (const auto& [render_order, pipeline, data] : unsorted_draw_calls) {
      if (render_order == i) {
        draw_calls.emplace_back(std::make_pair(pipeline, data));
      }
    }
  }

  /* begin rendering */
  render_data.draw_buffer = gfx_->BeginRender();

  /* these descriptor set bindings should persist across pipeline changes */
  const gfx::Pipeline* const first_pipeline = draw_calls.begin()->first;
  gfx_->CmdBindDescriptorSet(render_data.draw_buffer, first_pipeline,
                             render_data.global_set, 0);
  gfx_->CmdBindDescriptorSet(render_data.draw_buffer, first_pipeline,
                             render_data.frame_set, 1);

  gfx_->CmdBindPipeline(render_data.draw_buffer, first_pipeline);
  const gfx::Pipeline* last_bound_pipeline = first_pipeline;

  for (const auto& [pipeline, draw_call] : draw_calls) {
    if (pipeline != last_bound_pipeline) {
      gfx_->CmdBindPipeline(render_data.draw_buffer, pipeline);
      last_bound_pipeline = pipeline;
    }
    gfx_->CmdBindDescriptorSet(render_data.draw_buffer, pipeline,
                               draw_call.material_set, 2);
    gfx_->CmdPushConstants(render_data.draw_buffer, pipeline, 0,
                           sizeof(PushConstants), &draw_call.push_constants);
    if (draw_call.ib) {
      gfx_->CmdBindVertexBuffer(render_data.draw_buffer, 0, draw_call.vb);
      gfx_->CmdBindIndexBuffer(render_data.draw_buffer, draw_call.ib);
      gfx_->CmdDrawIndexed(render_data.draw_buffer, draw_call.index_count, 1, 0,
                           0, 0);
    } else {
      gfx_->CmdDraw(render_data.draw_buffer, draw_call.index_count, 1, 0, 0);
    }
  }

  /* draw */
  gfx_->FinishRender(render_data.draw_buffer);
}

void RenderSystem::SetCameraEntity(uint32_t entity) {
  camera_.cam_entity = entity;
}

}  // namespace engine
