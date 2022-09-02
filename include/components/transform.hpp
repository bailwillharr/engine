#pragma once

#include "component.hpp"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/ext/quaternion_float.hpp>

#include <vector>
#include <string>
#include <memory>

namespace components {

class Transform : public Component {

// Scale, rotate (XYZ), translate

private:
	glm::mat4 m_transformMatrix{1.0f};

public:
	Transform(Object*);
	~Transform() override;

	glm::vec3 position{0.0f};
	glm::quat rotation{};
	glm::vec3 scale{1.0f};

};

}
