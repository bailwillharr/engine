#pragma once

#include "ecs_system.h"
#include "event_system.h"

#include "components/transform.h"
#include "systems/collisions.h"

struct CameraControllerComponent {
	float m_cameraSensitivity = 0.007f;

	float m_yaw = 0.0f;
	float m_pitch = 0.0f;

	const float walk_speed = 4.0f;
	
	bool isGrounded = false;
	float dy = 0.0f;
	float standingHeight = 0.0f;
	const float thrust = 25.0f;

	glm::vec3 lastCollisionNormal{};
	glm::vec3 lastCollisionPoint{};
	bool justCollided = false;
};

class CameraControllerSystem : public engine::System, public engine::EventHandler<engine::PhysicsSystem::CollisionEvent> {

public:
	CameraControllerSystem(engine::Scene* scene);

	// engine::System overrides
	void OnUpdate(float ts) override;

	// engine::EventHandler overrides
	void OnEvent(engine::PhysicsSystem::CollisionEvent info) override;

	engine::TransformComponent* t = nullptr;
	engine::ColliderComponent* col = nullptr;
	CameraControllerComponent* c = nullptr;
};
