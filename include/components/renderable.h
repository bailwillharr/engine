#ifndef ENGINE_INCLUDE_COMPONENTS_RENDERABLE_H_
#define ENGINE_INCLUDE_COMPONENTS_RENDERABLE_H_

#include <memory>

#include "resources/material.h"
#include "resources/mesh.h"

namespace engine {

struct RenderableComponent {
  std::shared_ptr<resources::Mesh> mesh;
  std::shared_ptr<resources::Material> material;
  bool shown = true;
  uint32_t index_count_override =
      0;  // for shaders that don't use vertex/index buffers, 0 means ignored
};

}  // namespace engine

#endif