#pragma once

#include "ecs_system.hpp"
#include "components/transform.hpp"
#include "scene.hpp"
#include "log.hpp"

#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>

#include <typeinfo>
#include <string.h>

namespace engine {

	class TransformSystem : public System {

	public:
		TransformSystem(Scene* scene)
			: System(scene, { typeid(TransformComponent).hash_code() })
		{
		}

		void onUpdate(float ts) override
		{
			for (uint32_t entity : m_entities) {

				auto t = m_scene->getComponent<TransformComponent>(entity);

				TRACE("tag is {}", t->tag);

				glm::mat4 transform;
				
				// rotation
				transform = glm::mat4_cast(t->rotation);
				// position
				reinterpret_cast<glm::vec3&>(transform[3]) = t->position;
				// scale (effectively applied first)
				transform = glm::scale(transform, t->scale);

				if (t->parent != 0) {
					transform = m_scene->getComponent<TransformComponent>(t->parent)->worldMatrix * transform;
				}

				t->worldMatrix = transform;
			}

		}

		uint32_t getChildEntity(uint32_t parent, const std::string& tag)
		{
			for (uint32_t entity : m_entities) {
				auto t = m_scene->getComponent<TransformComponent>(entity);
				if (t->parent == parent) {
					if (t->tag == tag) {
						return entity;
					}
				}
			}
			return 0;
		}

	};

}

