#include "camera_controller.hpp"

#include "object.hpp"

#include "window.hpp"
#include "input.hpp"

#include <glm/trigonometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/rotate_vector.hpp>

#include <log.hpp>

CameraController::CameraController(engine::Object* parent) :
	CustomComponent(parent)
{
	standingHeight = parent->transform.position.y;
	m_yaw = glm::half_pi<float>();
}

void CameraController::onUpdate(glm::mat4 t)
{

	// calculate new position

	// use one unit per meter

	const float dt = win.dt();

	// jumping
	constexpr float G = 9.8f;
	constexpr float JUMPHEIGHT = 16.0f * 25.4f / 1000.0f; // 16 inches
	constexpr float JUMPVEL = (float)2.82231110971133017648; //std::sqrt(2 * G * JUMPHEIGHT);
	//constexpr float JUMPDURATION = 0.5f;
	//constexpr float JUMPVEL = G * JUMPDURATION / 2.0f;

	if (inp.getButton("jump") && isJumping == false) {
		isJumping = true;
		dy = JUMPVEL;
		//standingHeight = tcomp->position.y;
		
	}

	if (isJumping) {
		dy -= G * dt;
		parent.transform.position.y += dy * dt;
		if (parent.transform.position.y < standingHeight) {
			isJumping = false;
			dy = 0.0f;
			parent.transform.position.y = standingHeight;
		}
	}

	if (win.getButton(engine::inputs::MouseButton::M_LEFT)) {
		//standingHeight = tcomp->position.y;
		dy += dt * thrust;
		isJumping = true;
	}

	// in metres per second
	//constexpr float SPEED = 1.5f;
	float SPEED = walk_speed;
	if (win.getKey(engine::inputs::Key::LSHIFT)) SPEED *= 10.0f;

	const float dx = inp.getAxis("movex") * SPEED;
	const float dz = (-inp.getAxis("movey")) * SPEED;

	// calculate new pitch and yaw

	constexpr float MAX_PITCH = glm::half_pi<float>();
	constexpr float MIN_PITCH = -MAX_PITCH;

	float dPitch = inp.getAxis("looky") * -1.0f * m_cameraSensitivity;
	m_pitch += dPitch;
	if (m_pitch <= MIN_PITCH || m_pitch >= MAX_PITCH) {
		m_pitch -= dPitch;
	}
	m_yaw += inp.getAxis("lookx") * -1.0f * m_cameraSensitivity;

	// update position relative to camera direction in xz plane
	const glm::vec3 d2xRotated = glm::rotateY(glm::vec3{ dx, 0.0f, 0.0f }, m_yaw);
	const glm::vec3 d2zRotated = glm::rotateY(glm::vec3{ 0.0f, 0.0f, dz }, m_yaw);
	parent.transform.position += (d2xRotated + d2zRotated) * dt;
	parent.transform.position.y += dy * dt;

	// pitch quaternion
	const float halfPitch = m_pitch / 2.0f;
	glm::quat pitchQuat{};
	pitchQuat.x = glm::sin(halfPitch);
	pitchQuat.y = 0.0f;
	pitchQuat.z = 0.0f;
	pitchQuat.w = glm::cos(halfPitch);

	// yaw quaternion
	const float halfYaw = m_yaw / 2.0f;
	glm::quat yawQuat{};
	yawQuat.x = 0.0f;
	yawQuat.y = glm::sin(halfYaw);
	yawQuat.z = 0.0f;
	yawQuat.w = glm::cos(halfYaw);

	// update rotation
	parent.transform.rotation = yawQuat * pitchQuat;

	if (win.getKeyPress(engine::inputs::Key::P)) {
		std::string pos_string{
			 "x: " + std::to_string(parent.transform.position.x) +
			" y: " + std::to_string(parent.transform.position.y) +
			" z: " + std::to_string(parent.transform.position.z)
		};
#ifdef NDEBUG
		win.infoBox("POSITION", pos_string);
#endif
		INFO("position: " + pos_string);
	}

}
