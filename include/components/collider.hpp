#pragma once

#include <glm/vec3.hpp>
#include <cstdint>

namespace engine {

	struct AABB {
		glm::vec3 pos1;
		glm::vec3 pos2;
	};

	class PhysicsSystem;

	struct ColliderComponent  {
		friend PhysicsSystem;

		ColliderComponent()
		{

		}

		bool isStatic;
		AABB aabb;

		glm::vec3 getLastCollisionNormal() { return {0.0f, 1.0f, 0.0f}; }
		glm::vec3 getLastCollisionPoint() { return {}; }
		bool getIsColliding() { return true; }
		bool getJustUncollided() { return false; }

	};

}
