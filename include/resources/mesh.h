#ifndef ENGINE_INCLUDE_RESOURCES_MESH_H_
#define ENGINE_INCLUDE_RESOURCES_MESH_H_

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <vector>

#include "gfx.h"
#include "gfx_device.h"

namespace engine {

struct Vertex {
  glm::vec3 pos;
  glm::vec3 norm;
  glm::vec4 tangent; // w component flips binormal if -1. w should be 1 or -1
  glm::vec2 uv;
};

}  // namespace engine

namespace engine {

class Mesh {
 public:
  Mesh(GFXDevice* gfx, const std::vector<Vertex>& vertices);
  Mesh(GFXDevice* gfx, const std::vector<Vertex>& vertices,
       const std::vector<uint32_t>& indices)
      : gfx_(gfx) {
    InitMesh(vertices, indices);
  }
  ~Mesh();
  Mesh(const Mesh&) = delete;
  Mesh& operator=(const Mesh&) = delete;

  const gfx::Buffer* GetVB();
  const gfx::Buffer* GetIB();
  uint32_t GetCount();

 private:
  GFXDevice* const gfx_;

  const gfx::Buffer* vb_;
  const gfx::Buffer* ib_;
  uint32_t count_;

  void InitMesh(const std::vector<Vertex>& vertices,
                const std::vector<uint32_t>& indices);
};

}  // namespace engine

#endif