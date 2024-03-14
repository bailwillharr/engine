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

    // in metres per second
    float speed = c->kWalkSpeed;
    if (scene_->app()->input_manager()->GetButton("sprint")) speed *= 10.0f;

    float dx = scene_->app()->input_manager()->GetAxis("movex");
    float dy = scene_->app()->input_manager()->GetAxis("movey");

    // calculate new pitch and yaw

    constexpr float kMaxPitch = glm::pi<float>();
    constexpr float kMinPitch = 0.0f;

    float d_pitch = scene_->app()->input_manager()->GetAxis("looky") * -1.0f * c->kCameraSensitivity;
    c->pitch += d_pitch;
    if (c->pitch <= kMinPitch || c->pitch >= kMaxPitch) {
        c->pitch -= d_pitch;
    }
    c->yaw += scene_->app()->input_manager()->GetAxis("lookx") * -1.0f * c->kCameraSensitivity;

    // update position relative to camera direction in xz plane
    const glm::vec3 d2x_rotated = glm::rotateZ(glm::vec3{dx, 0.0f, 0.0f}, c->yaw);
    const glm::vec3 d2y_rotated = glm::rotateZ(glm::vec3{0.0f, dy, 0.0f}, c->yaw);
    glm::vec3 h_vel = (d2x_rotated + d2y_rotated);
    h_vel *= speed;
    t->position += h_vel * dt;

    // gravity stuff here:
    constexpr float g = -9.81f; // constant velocity gravity???
    constexpr float player_height = 71.0f * 25.4f / 1000.0f;
    c->vel.z += g * dt;

    // check for collision during next frame and push back and remove velocity in the normal of the collision direction if so
    engine::Ray ray{};
    ray.origin = t->position;
    ray.origin.z -= player_height; // check for collision from the player's feet

    ray.direction = c->
    const engine::Raycast fall_raycast = scene_->GetSystem<engine::CollisionSystem>()->GetRaycast(fall_ray);
    if (fall_raycast.hit && fall_raycast.distance < player_height + (-c->fall_vel * dt)) {
        t->position.z += -(fall_raycast.distance - player_height);
        c->fall_vel = 0.0f;
    }
    else {
        t->position.z += c->fall_vel * dt;
    }

    constexpr float kMaxDistanceFromOrigin = 10000.0f;

    if (glm::length(t->position) > kMaxDistanceFromOrigin) {
        t->position = {0.0f, 0.0f, 10.0f};
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

    if (scene_->app()->window()->GetButtonPress(engine::inputs::MouseButton::M_LEFT)) {
        engine::Ray ray{};
        ray.origin = t->position;
        ray.direction = glm::vec3(glm::mat4_cast(t->rotation) * glm::vec4{0.0f, 0.0f, -1.0f, 1.0f});
        engine::Raycast cast = scene_->GetSystem<engine::CollisionSystem>()->GetRaycast(ray);
        if (cast.hit) {
            LOG_INFO("Raycast hit {}", scene_->GetComponent<engine::TransformComponent>(cast.hit_entity)->tag);
            LOG_INFO("Distance: {} m", cast.distance);
        }
    }
}