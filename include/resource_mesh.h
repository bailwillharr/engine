#pragma once

#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "gfx.h"
#include "gfx_device.h"

namespace engine {

struct Vertex {
    glm::vec3 pos;
    glm::vec3 norm;
    glm::vec4 tangent; // w component flips binormal if -1. w should be 1 or -1
    glm::vec2 uv;

    static constexpr int floatsPerVertex() { return static_cast<int>(sizeof(Vertex) / sizeof(float)); }
};

class Mesh {
    GFXDevice* const m_gfx;
    const gfx::Buffer* m_vb;
    const gfx::Buffer* m_ib;
    uint32_t m_count;

public:
    Mesh(GFXDevice* gfx, const std::vector<Vertex>& vertices);
    Mesh(GFXDevice* gfx, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
    Mesh(const Mesh&) = delete;

    ~Mesh();

    Mesh& operator=(const Mesh&) = delete;

    const gfx::Buffer* getVB();
    const gfx::Buffer* getIB();
    uint32_t getCount();

private:
    void initMesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
};

} // namespace engine