#include "systems/collisions.h"

#include "components/transform.h"
#include "components/collider.h"
#include "components/mesh_renderable.h"
#include "scene.h"

#include "log.h"

#include <array>

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
		const int index = static_cast<int>(minDistance - distances.begin());
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
		scene_->event_system()->RegisterEventType<CollisionEvent>();
	}

	void PhysicsSystem::OnComponentInsert(Entity entity)
	{
		(void)entity;
		const size_t size = entities_.size();
		static_aabbs_.reserve(size);
		dynamic_aabbs_.reserve(size);
		possible_collisions_.reserve(size);
		collision_infos_.reserve(size);
		LOG_TRACE("added entity {} to collider system", entity);
	}

	void PhysicsSystem::OnUpdate(float ts)
	{
		(void)ts;

		static_aabbs_.clear();
		dynamic_aabbs_.clear();
		possible_collisions_.clear();
		collision_infos_.clear();

		for (Entity entity : entities_) {
			const auto t = scene_->GetComponent<TransformComponent>(entity);
			const auto c = scene_->GetComponent<ColliderComponent>(entity);

			const glm::vec3 globalPosition = t->world_matrix[3];
			const AABB localBoundingBox = c->aabb;
			AABB globalBoundingBox{};
			globalBoundingBox.pos1 = globalPosition + localBoundingBox.pos1;
			globalBoundingBox.pos2 = globalPosition + localBoundingBox.pos2;

			auto& a = globalBoundingBox;
			if (a.pos1.x > a.pos2.x) std::swap(a.pos1.x, a.pos2.x);
			if (a.pos1.y > a.pos2.y) std::swap(a.pos1.y, a.pos2.y);
			if (a.pos1.z > a.pos2.z) std::swap(a.pos1.z, a.pos2.z);

			if (c->is_static) {
				static_aabbs_.emplace_back(std::make_tuple(entity, globalBoundingBox, c->is_trigger));
			} else {
				dynamic_aabbs_.emplace_back(std::make_tuple(entity, globalBoundingBox, c->is_trigger));
			}
		}

		/* BROAD PHASE */

		// Check every static collider against every dynamic collider, and every dynamic collider against every other one
		// This technique is inefficient for many entities.
		for (const auto& [staticEntity, staticAABB, staticTrigger] : static_aabbs_) {
			for (const auto& [dynamicEntity, dynamicAABB, dynamicTrigger] : dynamic_aabbs_) {
				if (checkCollisionFast(staticAABB, dynamicAABB)) {
					if (staticTrigger || dynamicTrigger) { // only check collisions involved with triggers
						possible_collisions_.emplace_back(
							staticEntity, staticAABB, staticTrigger,
							dynamicEntity, dynamicAABB, dynamicTrigger
						);
					}
				}
			}
		}

		// get collision details and submit events
		for (const auto& possibleCollision : possible_collisions_) {
			if (possibleCollision.static_trigger) {
				CollisionEvent info{};
				info.is_collision_enter = true;
				info.collided_entity = possibleCollision.dynamic_entity;
				info.normal = getAABBNormal(possibleCollision.static_aabb, possibleCollision.dynamic_aabb);

				AABB object = possibleCollision.dynamic_aabb;
				info.point = object.pos2;

				collision_infos_.emplace_back(possibleCollision.static_entity, info);
			}
			if (possibleCollision.dynamic_trigger) {
				CollisionEvent info{};
				info.is_collision_enter = true;
				info.collided_entity = possibleCollision.static_entity;
				info.normal = getAABBNormal(possibleCollision.dynamic_aabb, possibleCollision.static_aabb);

				AABB object = possibleCollision.static_aabb;
				info.point = object.pos2;

				collision_infos_.emplace_back(possibleCollision.dynamic_entity, info);
			}
		}

		for (const auto& [entity, info] : collision_infos_) {
			scene_->event_system()->QueueEvent<CollisionEvent>(EventSubscriberKind::kEntity, entity, info);
		}
	}

}

