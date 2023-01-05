#pragma once

#include "gfx.hpp"
#include "gfx_device.hpp"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <string>

namespace engine {
	struct Vertex {
		glm::vec3 pos;
		glm::vec3 norm;
		glm::vec2 uv;
	};
}

namespace engine::resources {

class Mesh {

public:
	Mesh(GFXDevice* gfx, const std::vector<Vertex>& vertices)
		: m_gfx(gfx)
	{
		m_vb = m_gfx->createBuffer(gfx::BufferType::VERTEX, vertices.size() * sizeof(Vertex), vertices.data());

		std::vector<uint32_t> indices(m_count);
		for (uint32_t i = 0; i < m_count; i++) {
			indices[i] = i;
		}
		m_ib = m_gfx->createBuffer(gfx::BufferType::INDEX, indices.size() * sizeof(uint32_t), indices.data());

		m_count = indices.size();
	}
	Mesh(GFXDevice* gfx, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
		: m_gfx(gfx)
	{
		m_vb = m_gfx->createBuffer(gfx::BufferType::VERTEX, vertices.size() * sizeof(Vertex), vertices.data());
		m_ib = m_gfx->createBuffer(gfx::BufferType::INDEX, indices.size() * sizeof(uint32_t), indices.data());
		m_count = indices.size();
	}
	~Mesh()
	{
		m_gfx->destroyBuffer(m_ib);
		m_gfx->destroyBuffer(m_vb);
	}
	Mesh(const Mesh&) = delete;
	Mesh& operator=(const Mesh&) = delete;

	auto getVB() { return m_vb; }
	auto getIB() { return m_ib; }
	auto getCount() { return m_count; }

private:
	GFXDevice* const m_gfx;

	const gfx::Buffer* m_vb;
	const gfx::Buffer* m_ib;
	uint32_t m_count;

};

}
