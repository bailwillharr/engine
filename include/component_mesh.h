#pragma once

#include <memory>

#include "resource_material.h"
#include "resource_mesh.h"

namespace engine {

struct MeshRenderableComponent {
    std::shared_ptr<Mesh> mesh;
    std::shared_ptr<Material> material;
    bool visible = true; // for static meshes, changes to this may require the static render list to be rebuilt
};

} // namespace engine