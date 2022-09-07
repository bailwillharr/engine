#pragma once

#include "engine_api.h"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/ext/quaternion_float.hpp>

struct ENGINE_API Transform {

// Scale, rotate (XYZ), translate

	glm::vec3 position{0.0f};
	glm::quat rotation{};
	glm::vec3 scale{1.0f};

};