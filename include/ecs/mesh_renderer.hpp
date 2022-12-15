#pragma once

#include "ecs_system.hpp"

#include "resources/texture.hpp"
#include "log.hpp"

namespace engine::ecs {

	struct MeshRendererComponent {
		int number;
		const resources::Texture* texture;
	};

	class RendererSystem : public EcsSystem<MeshRendererComponent> {

	public:
		void onUpdate(float ts) override
		{
			for (const auto& [id, data] : m_components) {
				DEBUG("rendering entity {}\tts={}", id, ts);
				DEBUG("  with texture: {}, number = {}", (void*)data.texture, data.number);
			}
		}
	};



}
