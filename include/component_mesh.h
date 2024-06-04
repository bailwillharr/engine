#pragma once

#include <memory>

namespace engine {

class Mesh;     // forward-dec
class Material; // forward-dec

struct MeshRenderableComponent {
    std::shared_ptr<Mesh> mesh;
    std::shared_ptr<Material> material;
    bool visible = true; // for static meshes, changes to this may require the static render list to be rebuilt
};

} // namespace engine