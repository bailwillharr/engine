#include "camera_controller.hpp"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/trigonometric.hpp>

#include "application.h"
#include "components/collider.h"
#include "components/transform.h"
#include "input_manager.h"
#include "log.h"
#include "scene.h"
#include "scene_manager.h"
#include "window.h"

CameraControllerSystem::CameraControllerSystem(engine::Scene* scene)
    : System(scene, {typeid(engine::TransformComponent).hash_code(),
                     typeid(CameraControllerComponent).hash_code()}) {}

void CameraControllerSystem::OnUpdate(float ts) {
  if (t == nullptr || c == nullptr || col == nullptr) {
    for (uint32_t entity : entities_) {
      t = scene_->GetComponent<engine::TransformComponent>(entity);
      col = scene_->GetComponent<engine::ColliderComponent>(entity);
      c = scene_->GetComponent<CameraControllerComponent>(entity);
      break;
    }
    if (t == nullptr) return;
    if (c == nullptr) return;
    if (col == nullptr) return;
  }

  const float dt = ts;

  constexpr float G = 9.8f;
  const float kMaxSlopeAngle = glm::cos(glm::radians(20.0f));
  constexpr float kFloorSinkLevel =
      0.05f;  // how far into the floor to ground the player

  glm::vec3 norm = c->last_collision_normal;

  glm::vec3 dir =
      glm::normalize(glm::rotateY(glm::vec3{1.0f, 0.0f, 0.0f}, c->yaw) +
                     glm::rotateY(glm::vec3{0.0f, 0.0f, 1.0f}, c->yaw));
  const float slope = glm::dot(dir, norm);

  bool is_sliding = false;

  if (c->just_collided) {
    if (slope > kMaxSlopeAngle) {
      // slide across wall
      is_sliding = true;
    } else {
      if (c->dy < 0.0f && c->is_grounded == false) {
        // in the ground, push up a bit
        float floorY = c->last_collision_point.y;
        t->position.y = floorY + 1.5f - kFloorSinkLevel;
        c->dy = 0.0f;
        c->is_grounded = true;
      }
    }
  }

  if (c->just_collided == false && slope <= kMaxSlopeAngle) {
    // just stopped colliding with a floor collider
    c->is_grounded = false;
  }

  if (c->is_grounded == false) c->dy -= G * dt;

  // jumping
  constexpr float JUMPVEL =
      (float)2.82231110971133017648;  // std::sqrt(2 * G * JUMPHEIGHT);
  if (scene_->app()->input_manager()->GetButton("jump") &&
      c->is_grounded == true) {
    c->dy = JUMPVEL;
  }

  if (scene_->app()->window()->GetButton(engine::inputs::MouseButton::M_LEFT)) {
    c->dy += dt * c->kThrust;
  }

  // in metres per second
  float speed = c->kWalkSpeed;
  if (scene_->app()->input_manager()->GetButton("sprint")) speed *= 10.0f;

  float dx = scene_->app()->input_manager()->GetAxis("movex");
  float dz = (-scene_->app()->input_manager()->GetAxis("movey"));

  // calculate new pitch and yaw

  constexpr float kMaxPitch = glm::half_pi<float>();
  constexpr float kMinPitch = -kMaxPitch;

  float d_pitch = scene_->app()->input_manager()->GetAxis("looky") * -1.0f *
                  c->kCameraSensitivity;
  c->pitch += d_pitch;
  if (c->pitch <= kMinPitch || c->pitch >= kMaxPitch) {
    c->pitch -= d_pitch;
  }
  c->yaw += scene_->app()->input_manager()->GetAxis("lookx") * -1.0f *
            c->kCameraSensitivity;

  // update position relative to camera direction in xz plane
  const glm::vec3 d2x_rotated = glm::rotateY(glm::vec3{dx, 0.0f, 0.0f}, c->yaw);
  const glm::vec3 d2z_rotated = glm::rotateY(glm::vec3{0.0f, 0.0f, dz}, c->yaw);
  glm::vec3 h_vel = (d2x_rotated + d2z_rotated);
  if (is_sliding) {
    h_vel = glm::vec3{norm.z, 0.0f, -norm.x};
  }
  h_vel *= speed;
  t->position += h_vel * dt;
  t->position.y += c->dy * dt;

  constexpr float kMaxDistanceFromOrigin = 10000.0f;

  if (glm::length(t->position) > kMaxDistanceFromOrigin) {
    t->position = {0.0f, 5.0f, 0.0f};
    c->dy = 0.0f;
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
  yaw_quat.y = glm::sin(half_yaw);
  yaw_quat.z = 0.0f;
  yaw_quat.w = glm::cos(half_yaw);

  // update rotation
  t->rotation = yaw_quat * pitch_quat;

  /* user interface inputs */

  if (scene_->app()->window()->GetKeyPress(engine::inputs::Key::K_P)) {
    std::string pos_string{"x: " + std::to_string(t->position.x) +
                           " y: " + std::to_string(t->position.y) +
                           " z: " + std::to_string(t->position.z)};
    LOG_INFO("position {}", pos_string);
  }

  if (scene_->app()->window()->GetKeyPress(engine::inputs::Key::K_R)) {
    t->position = {0.0f, 5.0f, 0.0f};
    c->dy = 0.0f;
  }

  if (scene_->app()->input_manager()->GetButtonPress("fullscreen")) {
    scene_->app()->window()->ToggleFullscreen();
  }

  if (scene_->app()->input_manager()->GetButtonPress("exit")) {
    scene_->app()->window()->SetCloseFlag();
  }

  c->just_collided = false;
}

// called once per frame
void CameraControllerSystem::OnEvent(
    engine::PhysicsSystem::CollisionEvent info) {
  c->just_collided = info.is_collision_enter;
  c->last_collision_normal = info.normal;
  c->last_collision_point = info.point;
}
