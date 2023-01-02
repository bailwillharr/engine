#pragma once

#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>

#include <string>

namespace engine {

	struct TransformComponent {
		glm::vec3 position;
		glm::quat rotation;
		glm::vec3 scale;

		glm::mat4 worldMatrix;

		std::string tag;
		uint32_t parent;
	};

}
