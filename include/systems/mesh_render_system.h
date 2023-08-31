#ifndef ENGINE_INCLUDE_SYSTEMS_MESH_RENDER_SYSTEM_H_
#define ENGINE_INCLUDE_SYSTEMS_MESH_RENDER_SYSTEM_H_

#include <glm/mat4x4.hpp>

#include "ecs_system.h"
#include "scene.h"
#include "gfx.h"

namespace engine {

struct StaticRenderListEntry {
  const gfx::Pipeline* pipeline;
  const gfx::Buffer* vertex_buffer;
  const gfx::Buffer* index_buffer;
  const gfx::DescriptorSet* base_colour_texture;
  glm::mat4 model_matrix;
  uint32_t index_count;
};

using StaticRenderList = std::vector<StaticRenderListEntry>;

class MeshRenderSystem : public System {
 public:
  MeshRenderSystem(Scene* scene);
  ~MeshRenderSystem();

  void RebuildRenderList();
  const StaticRenderList* GetStaticRenderList() const { return &static_render_list_; }

  void OnComponentInsert(uint32_t entity) override;
  void OnUpdate(float ts) override;

 private:
  StaticRenderList static_render_list_;
  bool list_needs_rebuild_ = false;

};

}  // namespace engine

#endif