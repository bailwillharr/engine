#pragma once

#include <glm/mat4x4.hpp>

#include "ecs.h"
#include "scene.h"
#include "gfx.h"

namespace engine {

struct RenderListEntry {
    const gfx::Pipeline* pipeline;
    const gfx::Buffer* vertex_buffer;
    const gfx::Buffer* index_buffer;
    const gfx::DescriptorSet* material_set;
    glm::mat4 model_matrix;
    uint32_t index_count;
};

using RenderList = std::vector<RenderListEntry>;

class MeshRenderSystem : public System {
   public:
    MeshRenderSystem(Scene* scene);
    ~MeshRenderSystem();

    void RebuildStaticRenderList();
    const RenderList* GetStaticRenderList() const { return &static_render_list_; }
    const RenderList* GetDynamicRenderList() const { return &dynamic_render_list_; }

    void onComponentInsert(Entity entity) override;
    void onUpdate(float ts) override;

   private:
    RenderList static_render_list_;
    RenderList dynamic_render_list_;
    bool list_needs_rebuild_ = false;

    // with_static_entities = false, build list of dynamic meshes
    // with_static_entities = true, build list of static meshes
    void BuildRenderList(RenderList& render_list, bool with_static_entities);
};

} // namespace engine