#pragma once

#include <glm/vec3.hpp>

namespace engine {

struct AABB {
    glm::vec3 min;
    glm::vec3 max;
};

struct ColliderComponent {
    AABB aabb;
};

} // namespace engine