#if 0

#include "terrain.hpp"

#include "resources/mesh.hpp"

std::unique_ptr<engine::resources::Mesh> getChunkMesh(int x, int y)
{
	(void)x;
	(void)y;

	std::vector<Vertex> vertices{
		{{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
		{{1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
		{{0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}}
	};

	return std::make_unique<engine::resources::Mesh>(vertices);
}

#endif
