#include "resources/mesh.h"

#include "log.h"
#include "gfx_device.h"

namespace engine {

Mesh::Mesh(GFXDevice* gfx, const std::vector<Vertex>& vertices) : m_gfx(gfx)
{
    std::vector<uint32_t> indices(vertices.size());
    for (uint32_t i = 0; i < indices.size(); i++) {
        indices[i] = i;
    }
    initMesh(vertices, indices);
}

Mesh::Mesh(GFXDevice* gfx, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) : m_gfx(gfx) { initMesh(vertices, indices); }

Mesh::~Mesh()
{
    m_gfx->DestroyBuffer(m_ib);
    m_gfx->DestroyBuffer(m_vb);
    LOG_DEBUG("Destroyed mesh");
}

const gfx::Buffer* Mesh::getVB() { return m_vb; }

const gfx::Buffer* Mesh::getIB() { return m_ib; }

uint32_t Mesh::getCount() { return m_count; }

void Mesh::initMesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
{
    m_vb = m_gfx->CreateBuffer(gfx::BufferType::kVertex, vertices.size() * sizeof(Vertex), vertices.data());
    m_ib = m_gfx->CreateBuffer(gfx::BufferType::kIndex, indices.size() * sizeof(uint32_t), indices.data());
    m_count = (uint32_t)indices.size();
    LOG_DEBUG("Created mesh, vertices: {}, indices: {}", vertices.size(), indices.size());
}

} // namespace engine
