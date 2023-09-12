#ifndef ENGINE_INCLUDE_COMPONENTS_MESH_RENDERABLE_H_
#define ENGINE_INCLUDE_COMPONENTS_MESH_RENDERABLE_H_

#include <memory>

#include "resources/material.h"
#include "resources/mesh.h"

namespace engine {

struct MeshRenderableComponent {
  std::shared_ptr<resources::Mesh> mesh;
  std::shared_ptr<resources::Material> material;
};

}  // namespace engine

#endif