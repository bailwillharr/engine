#include "systems/physics.hpp"

#include "components/transform.hpp"
#include "components/collider.hpp"
#include "components/renderable.hpp"
#include "scene.hpp"

#include "log.hpp"

namespace engine {

	PhysicsSystem::PhysicsSystem(Scene* scene)
		: System(scene, { typeid(TransformComponent).hash_code(), typeid(ColliderComponent).hash_code() })
	{

	}

	void PhysicsSystem::onUpdate(float ts)
	{
		(void)ts;

		using glm::vec3;

		struct CollisionInfo {
			uint32_t entity;
			TransformComponent* t;
			ColliderComponent* c;

			vec3 pos;

			bool wasColliding;
		};

		std::vector<CollisionInfo> entityColliders(m_entities.size());

		uint32_t i = 0;
		for (uint32_t entity : m_entities) {
			auto t = m_scene->getComponent<TransformComponent>(entity);
			auto c = m_scene->getComponent<ColliderComponent>(entity);

			entityColliders[i].entity = entity;
			entityColliders[i].t = t;
			entityColliders[i].c = c;

			entityColliders[i].wasColliding = c->getIsColliding();
			c->m_isColliding = false;
			c->m_justCollided = false;
			c->m_justUncollided = false;

			vec3 pos = reinterpret_cast<vec3&>(t->worldMatrix[3]);
			entityColliders[i].pos = pos;

			i++;
		}

		for (size_t i = 0; i < entityColliders.size(); i++) {
			for (size_t j = i + 1; j < entityColliders.size(); j++) {
				const vec3 v = entityColliders[i].pos - entityColliders[j].pos;
				const float distanceSquared = v.x * v.x + v.y * v.y + v.z * v.z;
				const float sumOfRadii = entityColliders[i].c->r + entityColliders[j].c->r;
				const float sumOfRadiiSquared = sumOfRadii * sumOfRadii;

				if (distanceSquared < sumOfRadiiSquared) {
					entityColliders[i].c->m_isColliding = true;
					entityColliders[j].c->m_isColliding = true;
				}
			}

			if (entityColliders[i].wasColliding != entityColliders[i].c->getIsColliding()) {
				if (entityColliders[i].c->getIsColliding()) {
					entityColliders[i].c->m_justCollided = true;
				} else {
					entityColliders[i].c->m_justUncollided = true;
				}
			}

			if (entityColliders[i].c->getJustCollided()) {
				TRACE("'{}' has collided!", entityColliders[i].t->tag);
				auto r = m_scene->getComponent<RenderableComponent>(entityColliders[i].entity);
				if (r != nullptr) {
					r->shown = true;
				}
			}
			if (entityColliders[i].c->getJustUncollided()) {
				TRACE("'{}' has stopped colliding!", entityColliders[i].t->tag);
				auto r = m_scene->getComponent<RenderableComponent>(entityColliders[i].entity);
				if (r != nullptr) {
					r->shown = false;
				}
			}

		}

	}

}

