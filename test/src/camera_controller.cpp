#include "camera_controller.hpp"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/trigonometric.hpp>

#include "application.h"
#include "components/transform.h"
#include "ecs.h"
#include "input_manager.h"
#include "log.h"
#include "scene.h"
#include "scene_manager.h"
#include "window.h"
#include <systems/collisions.h>

CameraControllerSystem::CameraControllerSystem(engine::Scene* scene)
    : System(scene, {typeid(engine::TransformComponent).hash_code(), typeid(CameraControllerComponent).hash_code()})
{
}

void CameraControllerSystem::OnUpdate(float ts)
{
    if (t == nullptr || c == nullptr) {
        for (engine::Entity entity : entities_) {
            t = scene_->GetComponent<engine::TransformComponent>(entity);
            c = scene_->GetComponent<CameraControllerComponent>(entity);
            break;
        }
        if (t == nullptr) return;
        if (c == nullptr) return;
    }

    const float dt = ts;

    float dx = scene_->app()->input_manager()->GetAxis("movex") * CameraControllerComponent::kSpeedStrafe;
    float dy = scene_->app()->input_manager()->GetAxis("movey") * CameraControllerComponent::kSpeedForwardBack;

    // calculate new pitch and yaw

    float d_pitch = scene_->app()->input_manager()->GetAxis("looky") * -1.0f * CameraControllerComponent::kCameraSensitivity;
    c->pitch += d_pitch;
    if (c->pitch <= CameraControllerComponent::kMinPitch || c->pitch >= CameraControllerComponent::kMaxPitch) {
        c->pitch -= d_pitch;
    }
    c->yaw += scene_->app()->input_manager()->GetAxis("lookx") * -1.0f * CameraControllerComponent::kCameraSensitivity;

    // update position relative to camera direction in xz plane
    const glm::vec3 d2x_rotated = glm::rotateZ(glm::vec3{dx, 0.0f, 0.0f}, c->yaw);
    const glm::vec3 d2y_rotated = glm::rotateZ(glm::vec3{0.0f, dy, 0.0f}, c->yaw);
    glm::vec3 h_vel = (d2x_rotated + d2y_rotated);
    c->vel.x = h_vel.x;
    c->vel.y = h_vel.y;
    // keep vel.z as gravity can increase it every frame

    // gravity stuff here:
    c->vel.z += CameraControllerComponent::kGravAccel * dt;

    // jumping
    if (scene_->app()->input_manager()->GetButtonPress("jump") && c->grounded) {
        c->vel.z += 4.4f; // m/s
    }

    // update position with velocity:

    // check horizontal collisions first as otherwise the player may be teleported above a wall instead of colliding against it
    if (c->vel.x != 0.0f || c->vel.y != 0.0f) { // just in case, to avoid a ray with direction = (0,0,0)
        engine::Ray horiz_ray{};
        horiz_ray.origin = t->position;         // set origin to 'MaxStairHeight' units above player's feet
        horiz_ray.origin.z += CameraControllerComponent::kMaxStairHeight - CameraControllerComponent::kPlayerHeight;
        horiz_ray.direction.x = c->vel.x;
        horiz_ray.direction.y = c->vel.y; // this is normalized by GetRayCast()
        horiz_ray.direction.z = 0.0f;
        const engine::Raycast horiz_raycast = scene_->GetSystem<engine::CollisionSystem>()->GetRaycast(horiz_ray);
        if (horiz_raycast.hit) {
            const glm::vec2 norm_xy = glm::normalize(glm::vec2{horiz_raycast.normal.x, horiz_raycast.normal.y}) * -1.0f; // make it point towards object
            const glm::vec2 vel_xy = glm::vec2{ c->vel.x, c->vel.y };
            // find the extent of the player's velocity in the direction of the wall's normal vector
            const glm::vec2 partial_vel = norm_xy * glm::dot(norm_xy, vel_xy);
            const glm::vec2 partial_dX = partial_vel * dt;
            if (glm::length(partial_dX) > horiz_raycast.distance - CameraControllerComponent::kPlayerCollisionRadius) {
                // player will collide with wall
                // push player out of collision zone
                const glm::vec2 push_vector = glm::normalize(vel_xy) * fmaxf(CameraControllerComponent::kPlayerCollisionRadius, horiz_raycast.distance);
                t->position.x = horiz_raycast.location.x - push_vector.x;
                t->position.y = horiz_raycast.location.y - push_vector.y;
                c->vel.x -= partial_vel.x;
                c->vel.y -= partial_vel.y;
            }
        }
    }

    // check falling collisions
    if (c->vel.z < 0.0f) { // if falling
        engine::Ray fall_ray{};
        fall_ray.origin = t->position;
        fall_ray.origin.z += CameraControllerComponent::kMaxStairHeight - CameraControllerComponent::kPlayerHeight;
        fall_ray.direction = {0.0f, 0.0f, -1.0f}; // down
        const engine::Raycast fall_raycast = scene_->GetSystem<engine::CollisionSystem>()->GetRaycast(fall_ray);
        if (fall_raycast.hit) {                   // there is ground below player
            // find how far the player will move (downwards) if velocity is applied without collision
            const float mag_dz = fabsf(c->vel.z * dt);
            // check if the player will be less than 'height' units above the collided ground
            if (mag_dz > fall_raycast.distance - CameraControllerComponent::kMaxStairHeight) {
                LOG_INFO("HIT");
                // push player up to ground level and set as grounded
                t->position.z = fall_raycast.location.z + CameraControllerComponent::kPlayerHeight;
                c->vel.z = 0.0f;
                c->grounded = true;
            }
            else { // there is ground below player, however they are not grounded
                c->grounded = false;
            }
        }
        else { // there is no ground below player (THEY ARE FALLING INTO THE VOID)
            c->grounded = false;
        }
    }
    else if (c->vel.z > 0.0f) {
        c->grounded = false; // they are jumping
    }

    if (c->was_grounded != c->grounded) {
        LOG_INFO("GROUNDED? {}", c->grounded);
        c->was_grounded = c->grounded;
    }

    t->position += c->vel * dt;

    if (glm::length(t->position) > CameraControllerComponent::kMaxDistanceFromOrigin) {
        t->position = {0.0f, 0.0f, 100.0f};
    }

    /* ROTATION STUFF */

    // pitch quaternion
    const float half_pitch = c->pitch / 2.0f;
    glm::quat pitch_quat{};
    pitch_quat.x = glm::sin(half_pitch);
    pitch_quat.y = 0.0f;
    pitch_quat.z = 0.0f;
    pitch_quat.w = glm::cos(half_pitch);

    // yaw quaternion
    const float half_yaw = c->yaw / 2.0f;
    glm::quat yaw_quat{};
    yaw_quat.x = 0.0f;
    yaw_quat.y = 0.0f;
    yaw_quat.z = glm::sin(half_yaw);
    yaw_quat.w = glm::cos(half_yaw);

    // update rotation
    t->rotation = yaw_quat * pitch_quat;

    /* user interface inputs */

    if (scene_->app()->window()->GetKeyPress(engine::inputs::Key::K_P)) {
        std::string pos_string{"x: " + std::to_string(t->world_matrix[3][0]) + " y: " + std::to_string(t->world_matrix[3][1]) +
                               " z: " + std::to_string(t->world_matrix[3][2])};
        LOG_INFO("position {}", pos_string);
        LOG_INFO("rotation w: {} x: {} y: {} z: {}", t->rotation.w, t->rotation.x, t->rotation.y, t->rotation.z);
    }

    if (scene_->app()->window()->GetKeyPress(engine::inputs::Key::K_R)) {
        t->position = {0.0f, 0.0f, 10.0f};
        c->vel = {0.0f, 0.0f, 0.0f};
        c->pitch = glm::half_pi<float>();
        c->yaw = 0.0f;
    }

    if (scene_->app()->input_manager()->GetButtonPress("fullscreen")) {
        scene_->app()->window()->ToggleFullscreen();
    }

    if (scene_->app()->input_manager()->GetButtonPress("exit")) {
        scene_->app()->window()->SetCloseFlag();
    }

    if (scene_->app()->window()->GetKeyPress(engine::inputs::Key::K_F)) {
        scene_->app()->scene_manager()->SetActiveScene(next_scene_);
    }

    static std::vector<engine::Line> perm_lines{};

    if (scene_->app()->window()->GetButton(engine::inputs::MouseButton::M_LEFT)) {
        engine::Ray ray{};
        ray.origin = t->position;
        ray.direction = glm::vec3(glm::mat4_cast(t->rotation) * glm::vec4{0.0f, 0.0f, -1.0f, 1.0f});
        engine::Raycast cast = scene_->GetSystem<engine::CollisionSystem>()->GetRaycast(ray);

        if (cast.hit) {
            LOG_INFO("Distance: {} m", cast.distance);
            LOG_INFO("Location: {} {} {}", cast.location.x, cast.location.y, cast.location.z);
            LOG_INFO("Normal: {} {} {}", cast.normal.x, cast.normal.y, cast.normal.z);
            LOG_INFO("Ray direction: {} {} {}", ray.direction.x, ray.direction.y, ray.direction.z);
            LOG_INFO("Hit Entity: {}", scene_->GetComponent<engine::TransformComponent>(cast.hit_entity)->tag);
            perm_lines.emplace_back(ray.origin, cast.location, glm::vec3{0.0f, 0.0f, 1.0f});
        }
    }
    if (scene_->app()->window()->GetButtonPress(engine::inputs::MouseButton::M_RIGHT)) {
        engine::Ray horiz_ray{};
        horiz_ray.origin = t->position;         // set origin to 'MaxStairHeight' units above player's feet
        horiz_ray.origin.z += CameraControllerComponent::kMaxStairHeight - CameraControllerComponent::kPlayerHeight;
        horiz_ray.direction.x = c->vel.x;
        horiz_ray.direction.y = c->vel.y; // this is normalized by GetRayCast()
        horiz_ray.direction.z = 0.0f;
        const engine::Raycast cast = scene_->GetSystem<engine::CollisionSystem>()->GetRaycast(horiz_ray);
        if (cast.hit) {
            LOG_INFO("Distance: {} m", cast.distance);
            LOG_INFO("Location: {} {} {}", cast.location.x, cast.location.y, cast.location.z);
            LOG_INFO("Normal: {} {} {}", cast.normal.x, cast.normal.y, cast.normal.z);
            perm_lines.emplace_back(horiz_ray.origin, cast.location, glm::vec3{ 0.0f, 0.0f, 1.0f });
        }
    }

    scene_->app()->debug_lines.insert(scene_->app()->debug_lines.end(), perm_lines.begin(), perm_lines.end());
}
