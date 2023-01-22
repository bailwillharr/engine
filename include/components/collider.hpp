#pragma once

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

		bool getIsColliding() { return m_isColliding; }
		bool getJustCollided() { return m_justCollided; }
		bool getJustUncollided() { return m_justUncollided; }

	private:
		bool m_isColliding;
		bool m_justCollided;
		bool m_justUncollided;
	};

}
