#pragma once

#include "ecs_system.hpp"

struct CameraControllerComponent {
	float m_cameraSensitivity = 0.007f;

	float m_yaw = 0.0f;
	float m_pitch = 0.0f;

	const float walk_speed = 4.0f;
	
	bool isGrounded = false;
	float dy = 0.0f;
	float standingHeight = 0.0f;
	const float thrust = 25.0f;

};

class CameraControllerSystem : public engine::System {

public:
	CameraControllerSystem(engine::Scene* scene);

	void onUpdate(float ts) override;
};
