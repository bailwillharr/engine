#include "camera_controller.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/trigonometric.hpp>

#include "application.h"
#include "component_mesh.h"
#include "component_transform.h"
#include "ecs.h"
#include "input_manager.h"
#include "log.h"
#include "scene.h"
#include "scene_manager.h"
#include "system_collisions.h"
#include "system_mesh_render.h"
#include "window.h"

CameraControllerSystem::CameraControllerSystem(engine::Scene* scene)
    : System(scene, {typeid(engine::TransformComponent).hash_code(), typeid(CameraControllerComponent).hash_code()})
{
}

void CameraControllerSystem::onUpdate(float ts)
{
    if (t == nullptr || c == nullptr) {
        for (engine::Entity entity : m_entities) {
            t = m_scene->GetComponent<engine::TransformComponent>(entity);
            c = m_scene->GetComponent<CameraControllerComponent>(entity);
            break;
        }
        if (t == nullptr) return;
        if (c == nullptr) return;
    }

    const float dt = ts;

    float dx = m_scene->app()->getInputManager()->GetAxis("movex") * CameraControllerComponent::kSpeedStrafe;
    float dy = m_scene->app()->getInputManager()->GetAxis("movey") * CameraControllerComponent::kSpeedForwardBack;

    if (m_scene->app()->getInputManager()->GetButton("sprint")) {
        dy *= CameraControllerComponent::kSprintMultiplier;
    }

    // calculate new pitch and yaw

    float d_pitch = m_scene->app()->getInputManager()->GetAxis("looky") * -1.0f * CameraControllerComponent::kCameraSensitivity;
    c->pitch += d_pitch;
    if (c->pitch <= CameraControllerComponent::kMinPitch || c->pitch >= CameraControllerComponent::kMaxPitch) {
        c->pitch -= d_pitch;
    }
    c->yaw += m_scene->app()->getInputManager()->GetAxis("lookx") * -1.0f * CameraControllerComponent::kCameraSensitivity;

    // update position relative to camera direction in xz plane
    const glm::vec3 d2x_rotated = glm::rotateZ(glm::vec3{dx, 0.0f, 0.0f}, c->yaw);
    const glm::vec3 d2y_rotated = glm::rotateZ(glm::vec3{0.0f, dy, 0.0f}, c->yaw);
    glm::vec3 h_vel = (d2x_rotated + d2y_rotated);
    c->vel.x = h_vel.x;
    c->vel.y = h_vel.y;
    // keep vel.z as gravity can increase it every frame

    // gravity stuff here:
    c->vel.z += c->grav_accel * dt;

    // jumping
    if (m_scene->app()->getInputManager()->GetButtonPress("jump") && (c->grounded || c->noclip)) {
        c->vel.z += CameraControllerComponent::kJumpVelocity; // m/s
    }

    // update position with velocity:

    // check horizontal collisions first as otherwise the player may be teleported above a wall instead of colliding against it
    if ((c->vel.x != 0.0f || c->vel.y != 0.0f) && !c->noclip) { // just in case, to avoid a ray with direction = (0,0,0)

        std::array<engine::Raycast, CameraControllerComponent::kNumHorizontalRays> raycasts{};
        engine::Raycast* chosen_cast = nullptr; // nullptr means no hit at all

        float smallest_distance = std::numeric_limits<float>::infinity();
        for (std::size_t i = 0; i < raycasts.size(); ++i) {

            const float lerp_value = static_cast<float>(i) / (static_cast<float>(CameraControllerComponent::kNumHorizontalRays) - 1);

            engine::Ray ray{};
            ray.origin = t->position;
            ray.origin.z -= (CameraControllerComponent::kPlayerHeight - CameraControllerComponent::kMaxStairHeight) * lerp_value;
            ray.direction.x = c->vel.x;
            ray.direction.y = c->vel.y; // this is normalized by GetRayCast()
            ray.direction.z = 0.0f;
            raycasts[i] = m_scene->GetSystem<engine::CollisionSystem>()->GetRaycast(ray);

            if (raycasts[i].hit && raycasts[i].distance < smallest_distance) {
                smallest_distance = raycasts[i].distance;
                chosen_cast = &raycasts[i];
            }
        }

        if (chosen_cast != nullptr) {
            const glm::vec2 norm_xy = glm::normalize(glm::vec2{chosen_cast->normal.x, chosen_cast->normal.y}) * -1.0f; // make it point towards object
            const glm::vec2 vel_xy = glm::vec2{c->vel.x, c->vel.y};
            // find the extent of the player's velocity in the direction of the wall's normal vector
            const glm::vec2 partial_vel = norm_xy * glm::dot(norm_xy, vel_xy);
            const glm::vec2 partial_dX = partial_vel * dt;
            if (glm::length(partial_dX) > chosen_cast->distance - CameraControllerComponent::kPlayerCollisionRadius) {
                // player will collide with wall
                // push player out of collision zone
                const glm::vec2 push_vector = glm::normalize(vel_xy) * fmaxf(CameraControllerComponent::kPlayerCollisionRadius, chosen_cast->distance);
                t->position.x = chosen_cast->location.x - push_vector.x;
                t->position.y = chosen_cast->location.y - push_vector.y;
                c->vel.x -= partial_vel.x;
                c->vel.y -= partial_vel.y;
            }
        }
    }

    // check falling collisions
    if (!c->noclip) {
        if (c->vel.z < 0.0f) { // if falling
            engine::Ray fall_ray{};
            fall_ray.origin = t->position;
            fall_ray.origin.z += CameraControllerComponent::kMaxStairHeight - CameraControllerComponent::kPlayerHeight;
            fall_ray.direction = {0.0f, 0.0f, -1.0f}; // down
            const engine::Raycast fall_raycast = m_scene->GetSystem<engine::CollisionSystem>()->GetRaycast(fall_ray);
            if (fall_raycast.hit) { // there is ground below player
                // find how far the player will move (downwards) if velocity is applied without collision
                const float mag_dz = fabsf(c->vel.z * dt);
                // check if the player will be less than 'height' units above the collided ground
                if (mag_dz > fall_raycast.distance - CameraControllerComponent::kMaxStairHeight) {
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
            // check if intersection with ceiling
            engine::Ray jump_ray{};
            jump_ray.origin = t->position;
            jump_ray.direction = {0.0f, 0.0f, 1.0f};
            const engine::Raycast jump_raycast = m_scene->GetSystem<engine::CollisionSystem>()->GetRaycast(jump_ray);
            if (jump_raycast.hit) {
                // find how far the player will move (upwards) if velocity is applied without collision
                const float mag_dz = fabsf(c->vel.z * dt);
                // check if the player will be higher than the collided ground
                if (mag_dz > jump_raycast.distance - CameraControllerComponent::kPlayerCollisionRadius) {
                    // push player below ceiling
                    t->position.z = jump_raycast.location.z - CameraControllerComponent::kPlayerCollisionRadius;
                    c->vel.z = 0.0f;
                }
            }
        }
    }

    t->position += c->vel * dt;

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

    if (m_scene->app()->getWindow()->GetKeyPress(engine::inputs::Key::K_R) || glm::length(t->position) > CameraControllerComponent::kMaxDistanceFromOrigin) {
        t->position = {0.000001f, 0.000001f, 10.0f};
        c->vel = {0.0f, 0.0f, 0.0f};
        c->pitch = glm::half_pi<float>();
        c->yaw = 0.0f;
    }

    if (m_scene->app()->getInputManager()->GetButtonPress("fullscreen")) {
        m_scene->app()->getWindow()->ToggleFullscreen();
    }

    if (m_scene->app()->getInputManager()->GetButtonPress("exit")) {
        m_scene->app()->getWindow()->SetCloseFlag();
    }

    if (m_scene->app()->getWindow()->GetKeyPress(engine::inputs::Key::K_F)) {
        m_scene->app()->getSceneManager()->SetActiveScene(next_scene_);
    }

    if (m_scene->app()->getWindow()->GetKeyPress(engine::inputs::Key::K_Q)) {
        c->noclip ^= true;
    }

    if (m_scene->app()->getWindow()->GetKeyPress(engine::inputs::Key::K_T)) {
        if (c->grav_accel != 0.0f) {
            c->grav_accel = 0.0f;
        }
        else {
            c->grav_accel = -9.81f;
        }
    }

    if (m_scene->app()->getWindow()->GetButtonPress(engine::inputs::MouseButton::M_LEFT)) {
        engine::Ray ray{};
        ray.origin = t->position;
        ray.direction = glm::vec3(glm::mat4_cast(t->rotation) * glm::vec4{0.0f, 0.0f, -1.0f, 1.0f});
        engine::Raycast cast = m_scene->GetSystem<engine::CollisionSystem>()->GetRaycast(ray);

        if (cast.hit) {
            LOG_TRACE("Distance: {} m", cast.distance);
            LOG_TRACE("Location: {} {} {}", cast.location.x, cast.location.y, cast.location.z);
            LOG_TRACE("Normal: {} {} {}", cast.normal.x, cast.normal.y, cast.normal.z);
            LOG_TRACE("Ray direction: {} {} {}", ray.direction.x, ray.direction.y, ray.direction.z);
            LOG_TRACE("Hit Entity: {}", m_scene->GetComponent<engine::TransformComponent>(cast.hit_entity)->tag);
            c->perm_lines.clear();
            c->perm_lines.emplace_back(ray.origin, cast.location, glm::vec3{0.0f, 0.0f, 1.0f});
            m_scene->GetComponent<engine::MeshRenderableComponent>(cast.hit_entity)->visible ^= true;
            m_scene->GetSystem<engine::MeshRenderSystem>()->RebuildStaticRenderList();
        }
    }

    m_scene->app()->debug_lines.insert(m_scene->app()->debug_lines.end(), c->perm_lines.begin(), c->perm_lines.end());
}
