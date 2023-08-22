#ifndef ENGINE_INCLUDE_COMPONENTS_TRANSFORM_H_
#define ENGINE_INCLUDE_COMPONENTS_TRANSFORM_H_

#include <string>

#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace engine {

struct TransformComponent {
  glm::mat4 world_matrix;

  glm::quat rotation;
  glm::vec3 position;
  glm::vec3 scale;

  std::string tag;
  uint32_t parent;

  bool is_static;
};

}  // namespace engine

#endif