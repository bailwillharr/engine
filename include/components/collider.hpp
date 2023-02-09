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

		bool isStatic = true;
		bool isTrigger = false; // entity receives an event on collision enter and exit
		AABB aabb{};

		bool getIsColliding() { return isColliding; }

	private:
		bool isColliding = false;

	};

}
