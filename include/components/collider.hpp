#ifndef ENGINE_INCLUDE_COMPONENTS_COLLIDER_H_
#define ENGINE_INCLUDE_COMPONENTS_COLLIDER_H_

#include <glm/vec3.hpp>

#include <cstdint>

namespace engine {

struct AABB {
  glm::vec3 pos1;
  glm::vec3 pos2;
};

class PhysicsSystem;

struct ColliderComponent {
  friend PhysicsSystem;

  bool is_static = true;
  bool is_trigger =
      false;    // entity receives an event on collision enter and exit
  AABB aabb{};  // broad phase
};

}  // namespace engine

#endif