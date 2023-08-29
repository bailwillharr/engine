#include "systems/transform.h"

#include "scene.h"
#include "components/transform.h"

#include <typeinfo>

namespace engine {

TransformSystem::TransformSystem(Scene* scene)
    : System(scene, {typeid(TransformComponent).hash_code()}) {}

void TransformSystem::OnUpdate(float ts) {
  (void)ts;

  for (uint32_t entity : entities_) {
    auto t = scene_->GetComponent<TransformComponent>(entity);

    glm::mat4 transform;

    // rotation
    transform = glm::mat4_cast(t->rotation);
    // position
    reinterpret_cast<glm::vec3&>(transform[3]) = t->position;
    // scale (effectively applied first)
    transform = glm::scale(transform, t->scale);

    if (t->parent != 0) {
      auto parent_t = scene_->GetComponent<TransformComponent>(t->parent);
      transform = parent_t->world_matrix * transform;

      // (debug builds only) ensure static objects have static parents
      assert(t->is_static == false || parent_t->is_static == true);
    }

    t->world_matrix = transform;
  }
}

uint32_t TransformSystem::GetChildEntity(uint32_t parent,
                                         const std::string& tag) {
  for (uint32_t entity : entities_) {
    auto t = scene_->GetComponent<TransformComponent>(entity);
    if (t->parent == parent) {
      if (t->tag == tag) {
        return entity;
      }
    }
  }
  return 0;
}

}  // namespace engine
