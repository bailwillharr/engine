#pragma once

#include "ecs_system.hpp"
#include "scene.hpp"
#include "log.hpp"

#include "components/transform.hpp"
#include "components/renderable.hpp"

namespace engine {

	class RenderSystem : public System {

	public:
		RenderSystem(Scene* scene)
			: System(scene, { typeid(TransformComponent).hash_code(), typeid(RenderableComponent).hash_code() })
		{
		}

		void onUpdate(float ts) override
		{
			for (uint32_t entity : m_entities) {

				auto t = m_scene->getComponent<TransformComponent>(entity);

				TRACE("rendering {}", t->tag);

			}

		}

	};

}

