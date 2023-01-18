#pragma once

namespace engine {

	class PhysicsSystem;

	struct ColliderComponent  {
		friend PhysicsSystem;

		float r;

		bool getIsColliding() { return m_isColliding; }
		bool getJustCollided() { return m_justCollided; }
		bool getJustUncollided() { return m_justUncollided; }

	private:
		bool m_isColliding;
		bool m_justCollided;
		bool m_justUncollided;
	};

}
