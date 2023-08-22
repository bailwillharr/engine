#ifndef ENGINE_INCLUDE_COMPONENTS_COLLIDER_H_
#define ENGINE_INCLUDE_COMPONENTS_COLLIDER_H_

#include <glm/vec3.hpp>

#include <cstdint>

namespace engine {

struct AABB {
  glm::vec3 pos1;
  glm::vec3 pos2;
};

struct ColliderComponent {
  bool is_static;
  bool is_trigger;  // entity receives an event on collision enter and exit
  AABB aabb;  // broad phase
};

}  // namespace engine

#endif