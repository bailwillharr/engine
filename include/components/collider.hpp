#pragma once

#include <glm/vec3.hpp>
#include <cstdint>

namespace engine {

	class PhysicsSystem;

	enum class ColliderType {
		SPHERE,
		PLANE,
	};

	struct ColliderComponent  {
		friend PhysicsSystem;

		ColliderType colliderType;

		union {
			struct {
				float r;
			} sphereCollider;
		} colliders;

		auto getIsColliding() { return m_isColliding; }
		auto getJustCollided() { return m_justCollided; }
		auto getJustUncollided() { return m_justUncollided; }
		auto getLastEntityCollided() { return m_lastEntityCollided; }
		auto getLastCollisionNormal() { return m_lastCollisionNormal; }
		auto getLastCollisionPoint() { return m_lastCollisionPoint; }

	private:
		bool m_isColliding;
		bool m_justCollided;
		bool m_justUncollided;
		uint32_t m_lastEntityCollided;
		glm::vec3 m_lastCollisionNormal;
		glm::vec3 m_lastCollisionPoint;
	};

}
