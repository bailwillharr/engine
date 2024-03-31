#pragma once

#include <string>

#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "ecs.h"

namespace engine {

struct TransformComponent {
    std::string tag;

    glm::mat4 world_matrix;

    glm::quat rotation;
    glm::vec3 position;
    glm::vec3 scale;

    Entity parent;

    bool is_static;
};

} // namespace engine