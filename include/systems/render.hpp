#pragma once

#include "ecs_system.hpp"
#include "scene.hpp"
#include "log.hpp"

#include "components/transform.hpp"
#include "components/renderable.hpp"

namespace engine {

	class RenderSystem : public System {

	public:
		RenderSystem(Scene* scene);

		void onUpdate(float ts) override;

		void setCameraEntity(uint32_t entity);

	private:
		struct {
			// only uses transform component, which is required for all entities anyway
			uint32_t camEntity = 0;
			float verticalFovDegrees = 70.0f;
			float clipNear = 0.5f;
			float clipFar = 10000.0f;
		} m_camera;

		float m_viewportAspectRatio = 1.0f;

	};

}

