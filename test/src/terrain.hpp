#pragma once

#include <memory>

namespace engine::resources {
	class Mesh;
}

std::unique_ptr<engine::resources::Mesh> getChunkMesh(int x, int y);
