#pragma once

#include "scene.hpp"

#include "resources/shader.hpp"

#include <string>

namespace engine::util {

	uint32_t loadMeshFromFile(Scene* parent, const std::string& path);

}
