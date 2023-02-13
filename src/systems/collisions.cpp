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
		return (
			a.pos1.x <= b.pos2.x &&
			a.pos2.x >= b.pos1.x &&
			a.pos1.y <= b.pos2.y &&
			a.pos2.y >= b.pos1.y &&
			a.pos1.z <= b.pos2.z &&
			a.pos2.z >= b.pos1.z
		);
	}

	static glm::vec3 getAABBNormal(AABB subject, AABB object)
	{
		// get centre position of static entity:
		glm::vec3 subjectCentre = subject.pos1 + ((subject.pos2 - subject.pos1) / glm::vec3{ 2.0f, 2.0f, 2.0f });
		// find which face the centre is closest to:
		const float PXdistance = glm::abs(subjectCentre.x - object.pos2.x);
		const float NXdistance = glm::abs(subjectCentre.x - object.pos1.x);
		const float PYdistance = glm::abs(subjectCentre.y - object.pos2.y);
		const float NYdistance = glm::abs(subjectCentre.y - object.pos1.y);
		const float PZdistance = glm::abs(subjectCentre.z - object.pos2.z);
		const float NZdistance = glm::abs(subjectCentre.z - object.pos1.z);
		const std::array<float, 6> distances { PXdistance, NXdistance, PYdistance, NYdistance, PZdistance, NZdistance };
		const auto minDistance = std::min_element(distances.begin(), distances.end());
		const int index = minDistance - distances.begin();
		switch (index) {
			case 0:
				// P_X
				return {1.0f, 0.0f, 0.0f};
			case 1:
				// N_X
				return {-1.0f, 0.0f, 0.0f};
			case 2:
				// P_Y
				return {0.0f, 1.0f, 0.0f};
			case 3:
				// N_Y
				return {0.0f, -1.0f, 0.0f};
			case 4:
				// P_Z
				return {0.0f, 0.0f, 1.0f};
			case 5:
				// N_Z
				return {0.0f, 0.0f, -1.0f};
			default:
				throw std::runtime_error("wtf");
		}
	}

	// class methods

	PhysicsSystem::PhysicsSystem(Scene* scene)
		: System(scene, { typeid(TransformComponent).hash_code(), typeid(ColliderComponent).hash_code() })
	{
		m_scene->events()->registerEventType<CollisionEvent>();
	}

	void PhysicsSystem::onComponentInsert(uint32_t entity)
	{
		(void)entity;
		const size_t size = m_entities.size();
		m_staticAABBs.reserve(size);
		m_dynamicAABBs.reserve(size);
		m_possibleCollisions.reserve(size);
		m_collisionInfos.reserve(size);
		TRACE("added entity {} to collider system", entity);
	}

	void PhysicsSystem::onUpdate(float ts)
	{
		(void)ts;

		m_staticAABBs.clear();
		m_dynamicAABBs.clear();
		m_possibleCollisions.clear();
		m_collisionInfos.clear();

		for (uint32_t entity : m_entities) {
			const auto t = m_scene->getComponent<TransformComponent>(entity);
			const auto c = m_scene->getComponent<ColliderComponent>(entity);

			const glm::vec3 globalPosition = t->worldMatrix[3];
			const AABB localBoundingBox = c->aabb;
			AABB globalBoundingBox;
			globalBoundingBox.pos1 = globalPosition + localBoundingBox.pos1;
			globalBoundingBox.pos2 = globalPosition + localBoundingBox.pos2;

			auto& a = globalBoundingBox;
			if (a.pos1.x > a.pos2.x) std::swap(a.pos1.x, a.pos2.x);
			if (a.pos1.y > a.pos2.y) std::swap(a.pos1.y, a.pos2.y);
			if (a.pos1.z > a.pos2.z) std::swap(a.pos1.z, a.pos2.z);

			if (c->isStatic) {
				m_staticAABBs.emplace_back(std::make_tuple(entity, globalBoundingBox, c->isTrigger));
			} else {
				m_dynamicAABBs.emplace_back(std::make_tuple(entity, globalBoundingBox, c->isTrigger));
			}
		}

		/* BROAD PHASE */

		// Check every static collider against every dynamic collider, and every dynamic collider against every other one
		// This technique is inefficient for many entities.
		for (auto [staticEntity, staticAABB, staticTrigger] : m_staticAABBs) {
			for (auto [dynamicEntity, dynamicAABB, dynamicTrigger] : m_dynamicAABBs) {
				if (checkCollisionFast(staticAABB, dynamicAABB)) {
					if (staticTrigger || dynamicTrigger) { // only check collisions involved with triggers
						m_possibleCollisions.emplace_back(
							staticEntity, staticAABB, staticTrigger,
							dynamicEntity, dynamicAABB, dynamicTrigger
						);
					}
				}
			}
		}

		// get collision details and submit events
		for (auto possibleCollision : m_possibleCollisions) {
			if (possibleCollision.staticTrigger) {
				CollisionEvent info{};
				info.isCollisionEnter = true;
				info.collidedEntity = possibleCollision.dynamicEntity;
				info.normal = getAABBNormal(possibleCollision.staticAABB, possibleCollision.dynamicAABB);

				AABB object = possibleCollision.dynamicAABB;
				info.point = object.pos2;

				m_collisionInfos.emplace_back(possibleCollision.staticEntity, info);
			}
			if (possibleCollision.dynamicTrigger) {
				CollisionEvent info{};
				info.isCollisionEnter = true;
				info.collidedEntity = possibleCollision.staticEntity;
				info.normal = getAABBNormal(possibleCollision.dynamicAABB, possibleCollision.staticAABB);

				AABB object = possibleCollision.staticAABB;
				info.point = object.pos2;

				m_collisionInfos.emplace_back(possibleCollision.dynamicEntity, info);
			}
		}

		for (auto [entity, info] : m_collisionInfos) {
			m_scene->events()->queueEvent<CollisionEvent>(EventSubscriberKind::ENTITY, entity, info);
		}
	}

}

