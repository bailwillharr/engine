#pragma once

#include <cstdint>
#include <vector>

#include <glm/mat4x4.hpp>

#include "components/collider.hpp"
#include "ecs_system.hpp"

namespace engine {

	class PhysicsSystem : public System {

	public:
		PhysicsSystem(Scene* scene);

		void OnUpdate(float ts) override;

		void OnComponentInsert(uint32_t entity) override;

		struct CollisionEvent {
			bool isCollisionEnter; // false == collision exit
			uint32_t collidedEntity; // the entity that this entity collided with
			glm::vec3 normal; // the normal of the surface this entity collided with; ignored on collision exit
			glm::vec3 point; // where the collision was detected
		};
	
	private:

		// dynamic arrays to avoid realloc on every frame

		// entity, aabb, isTrigger
		std::vector<std::tuple<uint32_t, AABB, bool>> m_staticAABBs{};
		std::vector<std::tuple<uint32_t, AABB, bool>> m_dynamicAABBs{};

		struct PossibleCollision {

			PossibleCollision(uint32_t staticEntity, AABB staticAABB, bool staticTrigger, uint32_t dynamicEntity, AABB dynamicAABB, bool dynamicTrigger) :
				staticEntity(staticEntity),
				staticAABB(staticAABB),
				staticTrigger(staticTrigger),
				dynamicEntity(dynamicEntity),
				dynamicAABB(dynamicAABB),
				dynamicTrigger(dynamicTrigger) {}

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

