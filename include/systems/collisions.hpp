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

		struct CollisionEvent {
			bool isCollisionEnter; // false == collision exit
			uint32_t collidedEntity; // the entity that this entity collided with
			glm::vec3 normal; // the normal of the surface this entity collided with; ignored on collision exit
		};
	
	private:

		// dynamic arrays to avoid realloc on every frame

		// entity, aabb, isTrigger
		std::vector<std::tuple<uint32_t, AABB, bool>> m_staticAABBs{};
		std::vector<std::tuple<uint32_t, AABB, bool>> m_dynamicAABBs{};

		struct PossibleCollision {
			uint32_t staticEntity;
			AABB staticAABB;
			bool staticTrigger;
			uint32_t dynamicEntity;
			AABB dynamicAABB;
			bool dynamicTrigger;
		};
		std::vector<PossibleCollision> m_possibleCollisions{};
		std::vector<std::pair<uint32_t, CollisionEvent>> m_collisionInfos{}; // target entity, event info

	};

}

