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
			auto* ec1 = &entityColliders[i];

			for (size_t j = i + 1; j < entityColliders.size(); j++) {

				auto* ec2 = &entityColliders[j];

				if (	ec1->c->colliderType == ColliderType::SPHERE &&
						ec2->c->colliderType == ColliderType::SPHERE		) {
					const vec3 v = ec1->pos - ec2->pos;
					const float distanceSquared = v.x * v.x + v.y * v.y + v.z * v.z;
					const float sumOfRadii = ec1->c->colliders.sphereCollider.r + ec2->c->colliders.sphereCollider.r;
					const float sumOfRadiiSquared = sumOfRadii * sumOfRadii;
					if (distanceSquared < sumOfRadiiSquared) {
						ec1->c->m_isColliding = true;
						ec2->c->m_isColliding = true;
					}

				} else if (		(ec1->c->colliderType == ColliderType::PLANE &&
								 ec2->c->colliderType == ColliderType::SPHERE) ||
								(ec1->c->colliderType == ColliderType::SPHERE &&
								 ec2->c->colliderType == ColliderType::PLANE)) {
					CollisionInfo *plane, *sphere;
					if (ec1->c->colliderType == ColliderType::PLANE) {
						plane = ec1;
						sphere = ec2;
					} else {
						sphere = ec1;
						plane = ec2;
					}
					float distance = plane->pos.y - sphere->pos.y;
					if (distance < 0.0f) distance = -distance; // make positive
					if (distance < sphere->c->colliders.sphereCollider.r) {
						plane->c->m_isColliding = true;
						sphere->c->m_isColliding = true;
					}
				} else {
					throw std::runtime_error("Collision combination not supported!");
				}

			}

			if (ec1->wasColliding != ec1->c->getIsColliding()) {
				if (ec1->c->getIsColliding()) {
					ec1->c->m_justCollided = true;
				} else {
					ec1->c->m_justUncollided = true;
				}
			}
			if (ec1->c->getJustCollided()) {
				TRACE("'{}' has collided!", ec1->t->tag);
				auto r = m_scene->getComponent<RenderableComponent>(ec1->entity);
				if (r != nullptr) {
					r->shown = true;
				}
			}
			if (ec1->c->getJustUncollided()) {
				TRACE("'{}' has stopped colliding!", ec1->t->tag);
				auto r = m_scene->getComponent<RenderableComponent>(ec1->entity);
				if (r != nullptr) {
					r->shown = false;
				}
			}

		}

	}

}

