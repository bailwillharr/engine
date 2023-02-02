#pragma once

#include "ecs_system.hpp"

#include "components/collider.hpp"

#include <glm/mat4x4.hpp>

namespace engine {

	class PhysicsSystem : public System {

	public:
		PhysicsSystem(Scene* scene);

		void onUpdate(float ts) override;

		void onComponentInsert(uint32_t entity) override;
	
	private:

		// dyanmic arrays to avoid realloc on every frame
		struct CollisionInfo {
			uint32_t entity;
			AABB aabb;
			// output
			bool isMaybeColliding; // broad phase
			bool isColliding; // narrow phase
		};
		std::vector<CollisionInfo> m_staticInfos{};
		std::vector<CollisionInfo> m_dynamicInfos{};

	};

}

