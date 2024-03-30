#ifndef ENGINE_TEST_SRC_CAMERA_CONTROLLER_H_
#define ENGINE_TEST_SRC_CAMERA_CONTROLLER_H_

#include <glm/vec3.hpp>

#include "components/transform.h"
#include "application.h"
#include "ecs.h"

struct CameraControllerComponent {
    // looking
    static constexpr float kCameraSensitivity = 0.007f;
    static constexpr float kMaxPitch = glm::pi<float>();
    static constexpr float kMinPitch = 0.0f;

    // moving
    static constexpr float kSpeedForwardBack = 4.0f;
    static constexpr float kSpeedStrafe = 4.0f;
    static constexpr float kSprintMultiplier = 2.0f;
    static constexpr float kJumpHeight = /*4.4f*/ 8.8f;

    // collision
    static constexpr float kPlayerHeight = 2.0f; // 71.0f * 25.4f / 1000.0f;
    static constexpr float kPlayerCollisionRadius = 0.2f; // this should be greater than z_near
    static constexpr float kMaxStairHeight = 0.2f;
    static constexpr size_t kNumHorizontalRays = 20;
    
    static constexpr float kGravAccel = -9.81f;
    static constexpr float kMaxDistanceFromOrigin = 1000.0f;

    bool noclip = false;

    float yaw = 0.0f;
    float pitch = glm::half_pi<float>();
    glm::vec3 vel{0.0f, 0.0f, 0.0f};
    bool grounded = false;

    std::vector<engine::Line> perm_lines{}; // raycasting lines
};

class CameraControllerSystem : public engine::System {
   public:
    CameraControllerSystem(engine::Scene* scene);

    // engine::System overrides
    void OnUpdate(float ts) override;

    engine::TransformComponent* t = nullptr;
    CameraControllerComponent* c = nullptr;

    engine::Scene* next_scene_ = nullptr;
};

#endif