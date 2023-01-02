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

		void onUpdate(float ts) override;

	};

}

