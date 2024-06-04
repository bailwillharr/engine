#pragma once

#include <glm/vec3.hpp>

namespace engine {

struct DebugLine {
    glm::vec3 pos1;
    glm::vec3 pos2;
    glm::vec3 color;
};

} // namespace engine