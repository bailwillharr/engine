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

namespace engine::resources {

class ENGINE_API Mesh : public Resource {

public:
	Mesh(const std::vector<Vertex>& vertices);
	Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
	Mesh(const std::filesystem::path& resPath);
	~Mesh() override;

	std::vector<Vertex> m_vertices;
	std::vector<uint32_t> m_indices;

	const gfx::Buffer* vb;
	const gfx::Buffer* ib;

private:

	void initMesh();

};

}
