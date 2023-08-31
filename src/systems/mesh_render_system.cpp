#include "systems/mesh_render_system.h"

#include <algorithm>

#include "components/transform.h"
#include "components/mesh_renderable.h"
#include "log.h"

namespace engine {

MeshRenderSystem::MeshRenderSystem(Scene* scene)
    : System(scene, {typeid(TransformComponent).hash_code(),
                     typeid(RenderableComponent).hash_code()}) {}

MeshRenderSystem::~MeshRenderSystem() {}

void MeshRenderSystem::RebuildStaticRenderList() {
  BuildRenderList(static_render_list_, true);
  list_needs_rebuild_ = false;
}

void MeshRenderSystem::OnComponentInsert(uint32_t entity) {
  (void)entity;
  list_needs_rebuild_ = true;
}

void MeshRenderSystem::OnUpdate(float ts) {
  // do stuff
  (void)ts;
  if (list_needs_rebuild_) {
    RebuildStaticRenderList();
  }
  BuildRenderList(dynamic_render_list_, false);
}

void MeshRenderSystem::BuildRenderList(RenderList& render_list,
                                       bool with_static_entities) {
  render_list.clear();
  render_list.reserve(entities_.size());

  std::unordered_map<const gfx::Pipeline*, int> render_orders;

  for (uint32_t entity : entities_) {
    auto transform = scene_->GetComponent<engine::TransformComponent>(entity);

    if (transform->is_static != with_static_entities) continue;

    auto renderable = scene_->GetComponent<engine::RenderableComponent>(entity);

    const gfx::Pipeline* pipeline =
        renderable->material->GetShader()->GetPipeline();

    render_list.emplace_back(
        RenderListEntry{.pipeline = pipeline,
                        .vertex_buffer = renderable->mesh->GetVB(),
                        .index_buffer = renderable->mesh->GetIB(),
                        .base_colour_texture =
                            renderable->material->texture_->GetDescriptorSet(),
                        .model_matrix = transform->world_matrix,
                        .index_count = renderable->mesh->GetCount()});

    if (render_orders.contains(pipeline) == false) {
      render_orders.emplace(
          pipeline, renderable->material->GetShader()->GetRenderOrder());
    }
  }

  // sort the meshes by pipeline
  auto sort_by_pipeline = [&render_orders](const RenderListEntry& e1,
                                           const RenderListEntry& e2) -> bool {
    return (render_orders.at(e1.pipeline) < render_orders.at(e2.pipeline));
  };

  std::sort(render_list.begin(), render_list.end(), sort_by_pipeline);

#if 0
  LOG_TRACE("\nPRINTING RENDER LIST ({})\n", with_static_entities ? "STATIC" : "DYNAMIC");
  for (const auto& entry : render_list) {
    LOG_TRACE("pipeline: {}", static_cast<const void*>(entry.pipeline));
    LOG_TRACE("vertex_buffer: {}",
              static_cast<const void*>(entry.vertex_buffer));
    LOG_TRACE("index_buffer: {}", static_cast<const void*>(entry.index_buffer));
    LOG_TRACE("base_color_texture: {}",
              static_cast<const void*>(entry.base_colour_texture));
    LOG_TRACE("transform position: {}, {}, {}", entry.model_matrix[3][0],
              entry.model_matrix[3][1], entry.model_matrix[3][2]);
  }
  LOG_TRACE("\nRENDER LIST END\n");
#endif
}

}  // namespace engine
