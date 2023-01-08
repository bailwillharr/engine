#pragma once

#include "gfx.hpp"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <vector>

namespace engine {

	class GFXDevice;

	struct Vertex {
		glm::vec3 pos;
		glm::vec3 norm;
		glm::vec2 uv;
	};
}

namespace engine::resources {

class Mesh {

public:
	Mesh(GFXDevice* gfx, const std::vector<Vertex>& vertices);
	Mesh(GFXDevice* gfx, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
		: m_gfx(gfx)
	{
		initMesh(vertices, indices);
	}
	~Mesh();
	Mesh(const Mesh&) = delete;
	Mesh& operator=(const Mesh&) = delete;

	const gfx::Buffer* getVB();
	const gfx::Buffer* getIB();
	uint32_t getCount();

private:
	GFXDevice* const m_gfx;

	const gfx::Buffer* m_vb;
	const gfx::Buffer* m_ib;
	uint32_t m_count;

	void initMesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);

};

}
