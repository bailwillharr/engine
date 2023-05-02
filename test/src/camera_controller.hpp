#ifndef ENGINE_TEST_SRC_CAMERA_CONTROLLER_H_
#define ENGINE_TEST_SRC_CAMERA_CONTROLLER_H_

#include <glm/vec3.hpp>

#include "components/transform.h"
#include "ecs_system.h"
#include "event_system.h"
#include "systems/collisions.h"

struct CameraControllerComponent {
  static constexpr float kWalkSpeed = 4.0f;
  static constexpr float kThrust = 25.0f;
  static constexpr float kCameraSensitivity = 0.007f;

  float yaw = 0.0f;
  float pitch = 0.0f;
  float dy = 0.0f;

  glm::vec3 last_collision_normal{};
  glm::vec3 last_collision_point{};
  
  bool just_collided = false;
  bool is_grounded = false;

};

class CameraControllerSystem
    : public engine::System,
      public engine::EventHandler<engine::PhysicsSystem::CollisionEvent> {
 public:
  CameraControllerSystem(engine::Scene* scene);

  // engine::System overrides
  void OnUpdate(float ts) override;

  // engine::EventHandler overrides
  void OnEvent(engine::PhysicsSystem::CollisionEvent info) override;

  engine::TransformComponent* t = nullptr;
  engine::ColliderComponent* col = nullptr;
  CameraControllerComponent* c = nullptr;
};

#endif