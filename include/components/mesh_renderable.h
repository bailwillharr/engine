#ifndef ENGINE_INCLUDE_COMPONENTS_MESH_RENDERABLE_H_
#define ENGINE_INCLUDE_COMPONENTS_MESH_RENDERABLE_H_

#include <memory>

#include "resources/material.h"
#include "resources/mesh.h"

namespace engine {

struct MeshRenderableComponent {
  std::shared_ptr<Mesh> mesh;
  std::shared_ptr<Material> material;
  bool visible = true; // for static meshes, changes to this may require the static render list to be rebuilt
};

}  // namespace engine

#endif