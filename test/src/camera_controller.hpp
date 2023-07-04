#ifndef ENGINE_TEST_SRC_CAMERA_CONTROLLER_H_
#define ENGINE_TEST_SRC_CAMERA_CONTROLLER_H_

#include <glm/vec3.hpp>

#include "components/transform.h"
#include "ecs_system.h"

struct CameraControllerComponent {
  static constexpr float kWalkSpeed = 4.0f;
  static constexpr float kCameraSensitivity = 0.007f;
  float yaw = 0.0f;
  float pitch = 0.0f;
};

class CameraControllerSystem
    : public engine::System {
 public:
  CameraControllerSystem(engine::Scene* scene);

  // engine::System overrides
  void OnUpdate(float ts) override;

  engine::TransformComponent* t = nullptr;
  CameraControllerComponent* c = nullptr;
};

#endif