#include "systems/collisions.hpp"

#include "components/transform.hpp"
#include "components/collider.hpp"
#include "components/renderable.hpp"
#include "scene.hpp"

#include "log.hpp"

namespace engine {

	// static functions

	static bool checkCollisionFast(AABB a, AABB b)
	{
		if (a.pos1.x > a.pos2.x) std::swap(a.pos1.x, a.pos2.x);
		if (a.pos1.y > a.pos2.y) std::swap(a.pos1.y, a.pos2.y);
		if (a.pos1.z > a.pos2.z) std::swap(a.pos1.z, a.pos2.z);
		if (b.pos1.x > b.pos2.x) std::swap(b.pos1.x, b.pos2.x);
		if (b.pos1.y > b.pos2.y) std::swap(b.pos1.y, b.pos2.y);
		if (b.pos1.z > b.pos2.z) std::swap(b.pos1.z, b.pos2.z);

		return (
			a.pos1.x <= b.pos2.x &&
			a.pos2.x >= b.pos1.x &&
			a.pos1.y <= b.pos2.y &&
			a.pos2.y >= b.pos1.y &&
			a.pos1.z <= b.pos2.z &&
			a.pos2.z >= b.pos1.z
		);
	}

	// class methods

	PhysicsSystem::PhysicsSystem(Scene* scene)
		: System(scene, { typeid(TransformComponent).hash_code(), typeid(ColliderComponent).hash_code() })
	{
	}

	void PhysicsSystem::onComponentInsert(uint32_t entity)
	{
		(void)entity;
		m_staticInfos.reserve(m_entities.size());
		m_dynamicInfos.reserve(m_entities.size());
		TRACE("added entity {} to collider system", entity);
	}

	void PhysicsSystem::onUpdate(float ts)
	{
		(void)ts;

		m_staticInfos.clear();
		m_dynamicInfos.clear();

		TRACE("Getting collider entities:");
		for (uint32_t entity : m_entities) {
			TRACE("   has entity: {}", entity);
			const auto t = m_scene->getComponent<TransformComponent>(entity);
			const auto c = m_scene->getComponent<ColliderComponent>(entity);

			const glm::vec3 globalPosition = t->worldMatrix[3];
			const AABB localBoundingBox = c->aabb;
			AABB globalBoundingBox;
			globalBoundingBox.pos1 = globalPosition + localBoundingBox.pos1;
			globalBoundingBox.pos2 = globalPosition + localBoundingBox.pos2;
			TRACE("   global bounding box:");
			TRACE("       pos1: {} {} {}", globalBoundingBox.pos1.x, globalBoundingBox.pos1.y, globalBoundingBox.pos1.z);
			TRACE("       pos2: {} {} {}", globalBoundingBox.pos2.x, globalBoundingBox.pos2.y, globalBoundingBox.pos2.z);

			CollisionInfo info{
				.entity = entity,
				.aabb = globalBoundingBox,
				.isMaybeColliding = false,
				.isColliding = false
			};
			if (c->isStatic) {
				m_staticInfos.push_back(info);
			} else {
				m_dynamicInfos.push_back(info);
			}
		}

		/* BROAD PHASE */

		TRACE("Starting broad phase collision test");

		struct PossibleCollision {

		};

		// Check every static collider against every dynamic collider, and every dynamic collider against every other one
		// This technique is inefficient for many entities.
		for (size_t i = 0; i < m_staticInfos.size(); i++) {
			for (size_t j = 0; j < m_dynamicInfos.size(); j++) {
				if (checkCollisionFast(m_staticInfos[i].aabb, m_dynamicInfos[j].aabb)) {
					m_staticInfos[i].isMaybeColliding = true;
					m_dynamicInfos[i].isMaybeColliding = true;
					TRACE("Collision detected between {} and {}", m_staticInfos[i].entity, m_dynamicInfos[j].entity);
				}
			}
		}
	}

}

