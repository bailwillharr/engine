#include "system_mesh_render.h"

#include <algorithm>

#include "component_mesh.h"
#include "component_transform.h"
#include "log.h"
#include "resource_material.h"

namespace engine {

MeshRenderSystem::MeshRenderSystem(Scene* scene) : System(scene, {typeid(TransformComponent).hash_code(), typeid(MeshRenderableComponent).hash_code()}) {}

MeshRenderSystem::~MeshRenderSystem() {}

void MeshRenderSystem::RebuildStaticRenderList()
{
    BuildRenderList(static_render_list_, true);
    list_needs_rebuild_ = false;
}

void MeshRenderSystem::onComponentInsert(Entity entity)
{
    (void)entity;
    list_needs_rebuild_ = true;
}

void MeshRenderSystem::onUpdate(float ts)
{
    // do stuff
    (void)ts;
    // update the static render list only if it needs updating
    if (list_needs_rebuild_) {
        RebuildStaticRenderList();
    }
    // update the dynamic render list always
    BuildRenderList(dynamic_render_list_, false);
}

void MeshRenderSystem::BuildRenderList(RenderList& render_list, bool with_static_entities)
{
    render_list.clear();
    render_list.reserve(m_entities.size());

    std::unordered_map<const gfx::Pipeline*, int> render_orders;

    for (Entity entity : m_entities) {
        auto transform = m_scene->GetComponent<engine::TransformComponent>(entity);

        if (transform->is_static != with_static_entities) continue;

        auto renderable = m_scene->GetComponent<engine::MeshRenderableComponent>(entity);

        if (renderable->visible == false) continue;

        const gfx::Pipeline* pipeline = renderable->material->getShader()->GetPipeline();

        render_list.emplace_back(RenderListEntry{.pipeline = pipeline,
                                                 .vertex_buffer = renderable->mesh->getVB(),
                                                 .index_buffer = renderable->mesh->getIB(),
                                                 .material_set = renderable->material->getDescriptorSet(),
                                                 .model_matrix = transform->world_matrix,
                                                 .index_count = renderable->mesh->getCount()});

        if (render_orders.contains(pipeline) == false) {
            render_orders.emplace(pipeline, renderable->material->getShader()->GetRenderOrder());
        }
    }

    // sort the meshes by pipeline
    auto sort_by_pipeline = [&render_orders](const RenderListEntry& e1, const RenderListEntry& e2) -> bool {
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

} // namespace engine
