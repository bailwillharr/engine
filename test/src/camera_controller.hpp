#ifndef ENGINE_TEST_SRC_CAMERA_CONTROLLER_H_
#define ENGINE_TEST_SRC_CAMERA_CONTROLLER_H_

#include <glm/vec3.hpp>

#include "components/transform.h"
#include "ecs.h"

struct CameraControllerComponent {
  static constexpr float kSpeedForwardBack = 4.0f;
  static constexpr float kSpeedStrafe = 4.0f;
  static constexpr float kCameraSensitivity = 0.007f;
  static constexpr float kMaxDistanceFromOrigin = 10000.0f;
  static constexpr float kGravAccel = -9.81f;
  static constexpr float kPlayerHeight = 71.0f * 25.4f / 1000.0f;
  static constexpr float kMaxStairHeight = 0.2f;
  static constexpr float kPlayerCollisionRadius = 0.2f;
  static constexpr float kMaxPitch = glm::pi<float>();
  static constexpr float kMinPitch = 0.0f;
  float yaw = 0.0f;
  float pitch = glm::half_pi<float>();
  glm::vec3 vel{ 0.0f, 0.0f, 0.0f };
  bool grounded = false;
  bool was_grounded = false;
};

class CameraControllerSystem
    : public engine::System {
 public:
  CameraControllerSystem(engine::Scene* scene);

  // engine::System overrides
  void OnUpdate(float ts) override;

  engine::TransformComponent* t = nullptr;
  CameraControllerComponent* c = nullptr;

  engine::Scene* next_scene_ = nullptr;
};

#endif