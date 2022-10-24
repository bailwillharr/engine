#pragma once

#include "engine_api.h"

#include "resource.hpp"

#include "gfx.hpp"

#include <glm/vec3.hpp>
#include <glm/vec2.hpp>

#include <vector>
#include <memory>

struct Vertex {
	glm::vec3 pos;
	glm::vec3 norm;
	glm::vec2 uv;
};

namespace engine {
	class GFXDevice;
}

namespace engine::resources {

class ENGINE_API Mesh : public Resource {

public:
	Mesh(GFXDevice* gfx, const std::vector<Vertex>& vertices);
	Mesh(GFXDevice* gfx, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
	Mesh(GFXDevice* gfx, const std::filesystem::path& resPath);
	~Mesh() override;

	void drawMesh(const gfx::Pipeline* pipeline);

	std::vector<Vertex> m_vertices;
	std::vector<uint32_t> m_indices;

private:
	const gfx::Buffer* vb;
	const gfx::Buffer* ib;

	GFXDevice* gfx;

	void initMesh();

};

}
