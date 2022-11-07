#pragma once

#include "components/custom.hpp"

class CameraController : public engine::components::CustomComponent {

public:
	CameraController(engine::Object* parent);
	void onUpdate(glm::mat4 t) override;

	float m_cameraSensitivity = 0.007f;

private:
	float m_yaw = 0.0f;
	float m_pitch = 0.0f;

	float walk_speed = 4.0f;
	
	bool isJumping = false;
	float dy = 0.0f;
	float standingHeight = 0.0f;
	float thrust = 25.0f;

};
