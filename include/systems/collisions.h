#ifndef ENGINE_INCLUDE_SYSTEMS_COLLISIONS_H_
#define ENGINE_INCLUDE_SYSTEMS_COLLISIONS_H_

#include <cstdint>
#include <vector>

#include <glm/mat4x4.hpp>

#include "components/collider.h"
#include "ecs.h"

namespace engine {

class PhysicsSystem : public System {
 public:
  PhysicsSystem(Scene* scene);

  void OnUpdate(float ts) override;

  void OnComponentInsert(Entity entity) override;

  struct CollisionEvent {
    bool is_collision_enter;   // false == collision exit
    Entity collided_entity;  // the entity that this entity collided with
    glm::vec3 normal;  // the normal of the surface this entity collided with;
                       // ignored on collision exit
    glm::vec3 point;   // where the collision was detected
  };

 private:
  // dynamic arrays to avoid realloc on every frame
  // entity, aabb, is_trigger
  std::vector<std::tuple<Entity, AABB, bool>> static_aabbs_{};
  std::vector<std::tuple<Entity, AABB, bool>> dynamic_aabbs_{};

  struct PossibleCollision {
    PossibleCollision(Entity static_entity, AABB static_aabb,
                      bool static_trigger, Entity dynamic_entity,
                      AABB dynamic_aabb, bool dynamic_trigger)
        : static_entity(static_entity),
          static_aabb(static_aabb),
          static_trigger(static_trigger),
          dynamic_entity(dynamic_entity),
          dynamic_aabb(dynamic_aabb),
          dynamic_trigger(dynamic_trigger) {}

    Entity static_entity;
    AABB static_aabb;
    bool static_trigger;
    Entity dynamic_entity;
    AABB dynamic_aabb;
    bool dynamic_trigger;
  };
  std::vector<PossibleCollision> possible_collisions_{};
  std::vector<std::pair<Entity, CollisionEvent>>
      collision_infos_{};  // target entity, event info
};

}  // namespace engine

#endif