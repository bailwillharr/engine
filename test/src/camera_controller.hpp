#pragma once

#include <glm/vec3.hpp>

#include "application.h"
#include "component_transform.h"
#include "debug_line.h"
#include "ecs.h"
#include "si.h"

struct CameraControllerComponent {
    // looking
    static constexpr float kCameraSensitivity = 0.003f;
    static constexpr float kMaxPitch = glm::pi<float>();
    static constexpr float kMinPitch = 0.0f;

    // moving
    static constexpr float kSpeedForwardBack = 4.0f;
    static constexpr float kSpeedStrafe = 4.0f;
    static constexpr float kSprintMultiplier = 2.0f;
    static constexpr float kJumpVelocity = 4.4f * 2.0f;

    // collision
    static constexpr float kPlayerHeight = engine::inchesToMeters(71.0f);
    static constexpr float kPlayerCollisionRadius = 0.2f; // this should be greater than z_near
    static constexpr float kMaxStairHeight = 0.2f;
    static constexpr std::size_t kNumHorizontalRays = 20;

    float grav_accel = -9.81f * 2.0f;
    // grav_accel = -1.625f; // moon gravity
    static constexpr float kMaxDistanceFromOrigin = 1.0e6f;

    bool noclip = false;

    float yaw = 0.0f;
    // float pitch = glm::half_pi<float>();
    float pitch = 0.0f;
    glm::vec3 vel{0.0f, 0.0f, 0.0f};
    bool grounded = false;

    std::vector<engine::DebugLine> perm_lines{}; // raycasting lines
};

class CameraControllerSystem : public engine::System {
public:
    CameraControllerSystem(engine::Scene* scene);

    // engine::System overrides
    void onUpdate(float ts) override;

    engine::TransformComponent* t = nullptr;
    CameraControllerComponent* c = nullptr;

    engine::Scene* next_scene_ = nullptr;
};
