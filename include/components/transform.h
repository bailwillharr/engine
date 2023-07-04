#ifndef ENGINE_INCLUDE_COMPONENTS_TRANSFORM_H_
#define ENGINE_INCLUDE_COMPONENTS_TRANSFORM_H_

#include <string>

#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace engine {

struct TransformComponent {
  glm::vec3 position;
  glm::quat rotation;
  glm::vec3 scale;

  glm::mat4 world_matrix;

  std::string tag;
  uint32_t parent;
};

}  // namespace engine

#endif