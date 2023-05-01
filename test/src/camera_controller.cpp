#include "camera_controller.hpp"

#include "application.hpp"
#include "window.hpp"
#include "input_manager.hpp"
#include "scene_manager.hpp"
#include "scene.hpp"

#include "components/transform.hpp"
#include "components/collider.hpp"

#include <glm/trigonometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/rotate_vector.hpp>

#include <log.hpp>

CameraControllerSystem::CameraControllerSystem(engine::Scene* scene)
	: System(scene, { typeid(engine::TransformComponent).hash_code(), typeid(CameraControllerComponent).hash_code() })
{
}

void CameraControllerSystem::OnUpdate(float ts)
{

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
	// calculate new position

	// use one unit per meter

	const float dt = ts;

	constexpr float G = 9.8f;
	const float MAX_SLOPE_ANGLE = glm::cos(glm::radians(20.0f));
//	constexpr float MAX_SLOPE_ANGLE = glm::radians(1000.0f); // treat every collider as a floor, (TODO: get wall collisions working so this can be removed)
	constexpr float FLOOR_SINK_LEVEL = 0.05f; // how far into the floor to ground the player

	glm::vec3 norm = c->lastCollisionNormal;

	glm::vec3 dir = glm::normalize(glm::rotateY(glm::vec3{ 1.0f, 0.0f, 0.0f }, c->m_yaw) + glm::rotateY(glm::vec3{ 0.0f, 0.0f, 1.0f }, c->m_yaw));
	const float slope = glm::dot(dir, norm);

	bool isSliding = false;

	if (c->justCollided) {
		if (slope > MAX_SLOPE_ANGLE) {
			// slide across wall
			isSliding = true;
		} else {
			if (c->dy < 0.0f && c->isGrounded == false) {
				// in the ground, push up a bit
				float floorY = c->lastCollisionPoint.y;
				t->position.y = floorY + 1.5f - FLOOR_SINK_LEVEL;
				c->dy = 0.0f;
				c->isGrounded = true;
			}
		}
	}

	if (c->justCollided == false && slope <= MAX_SLOPE_ANGLE) {
		// just stopped colliding with a floor collider
		c->isGrounded = false;
	}

	if (c->isGrounded == false)
		c->dy -= G * dt;

	// jumping
	constexpr float JUMPVEL = (float)2.82231110971133017648; //std::sqrt(2 * G * JUMPHEIGHT);
	if (scene_->app()->input_manager()->GetButton("jump") && c->isGrounded == true) {
		c->dy = JUMPVEL;
	}

	if (scene_->app()->window()->GetButton(engine::inputs::MouseButton::M_LEFT)) {
		c->dy += dt * c->thrust;
	}

	// in metres per second
	float SPEED = c->walk_speed;
	if (scene_->app()->input_manager()->GetButton("sprint")) SPEED *= 10.0f;

	float dx = scene_->app()->input_manager()->GetAxis("movex");
	float dz = (-scene_->app()->input_manager()->GetAxis("movey"));

	// calculate new pitch and yaw

	constexpr float MAX_PITCH = glm::half_pi<float>();
	constexpr float MIN_PITCH = -MAX_PITCH;

	float dPitch = scene_->app()->input_manager()->GetAxis("looky") * -1.0f * c->m_cameraSensitivity;
	c->m_pitch += dPitch;
	if (c->m_pitch <= MIN_PITCH || c->m_pitch >= MAX_PITCH) {
		c->m_pitch -= dPitch;
	}
	c->m_yaw += scene_->app()->input_manager()->GetAxis("lookx") * -1.0f * c->m_cameraSensitivity;

	// update position relative to camera direction in xz plane
	const glm::vec3 d2xRotated = glm::rotateY(glm::vec3{ dx, 0.0f, 0.0f }, c->m_yaw);
	const glm::vec3 d2zRotated = glm::rotateY(glm::vec3{ 0.0f, 0.0f, dz }, c->m_yaw);
	glm::vec3 hVel = (d2xRotated + d2zRotated);
	if (isSliding) {
		hVel = glm::vec3{norm.z, 0.0f, -norm.x};
	}
	hVel *= SPEED;
	t->position += hVel * dt;
	t->position.y += c->dy * dt;

	constexpr float MAX_DISTANCE_FROM_ORIGIN = 10000.0f;

	if (glm::length(t->position) > MAX_DISTANCE_FROM_ORIGIN) {
		t->position = { 0.0f, 5.0f, 0.0f };
		c->dy = 0.0f;
	}

	/* ROTATION STUFF */

	// pitch quaternion
	const float halfPitch = c->m_pitch / 2.0f;
	glm::quat pitchQuat{};
	pitchQuat.x = glm::sin(halfPitch);
	pitchQuat.y = 0.0f;
	pitchQuat.z = 0.0f;
	pitchQuat.w = glm::cos(halfPitch);

	// yaw quaternion
	const float halfYaw = c->m_yaw / 2.0f;
	glm::quat yawQuat{};
	yawQuat.x = 0.0f;
	yawQuat.y = glm::sin(halfYaw);
	yawQuat.z = 0.0f;
	yawQuat.w = glm::cos(halfYaw);

	// update rotation
	t->rotation = yawQuat * pitchQuat;

	

	/* user interface inputs */

	if (scene_->app()->window()->GetKeyPress(engine::inputs::Key::K_P)) {
		std::string pos_string{
			 "x: " + std::to_string(t->position.x) +
			" y: " + std::to_string(t->position.y) +
			" z: " + std::to_string(t->position.z)
		};
		//scene_->app()->window()->InfoBox("POSITION", pos_string);
		LOG_INFO("position: " + pos_string);
	}

	if (scene_->app()->window()->GetKeyPress(engine::inputs::Key::K_R)) {
		t->position = { 0.0f, 5.0f, 0.0f };
		c->dy = 0.0f;
	}

	if (scene_->app()->input_manager()->GetButtonPress("fullscreen")) {
		scene_->app()->window()->ToggleFullscreen();
	}

	if (scene_->app()->input_manager()->GetButtonPress("exit")) {
		scene_->app()->window()->SetCloseFlag();
	}

	c->justCollided = false;

}

// called once per frame
void CameraControllerSystem::OnEvent(engine::PhysicsSystem::CollisionEvent info)
{
	c->justCollided = info.isCollisionEnter;
	c->lastCollisionNormal = info.normal;
	c->lastCollisionPoint = info.point;
}
