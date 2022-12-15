#pragma once

#include "ecs_system.hpp"
#include "components/mesh_renderer.hpp"

#include "log.hpp"

#include <cstdint>

namespace engine {

	class RendererSystem : public ecs::System<components::MeshRenderer> {

	public:
		void onUpdate(float ts) override
		{
			for (const auto& [id, data] : m_components) {
				DEBUG("rendering entity {}\tts={}", id, ts);
			}
		}
	};

	class Scene {
	
	public:
		Scene();
		Scene(const Scene&) = delete;
		Scene& operator=(const Scene&) = delete;
		~Scene();

		void update(float ts);

		uint32_t createEntity()
		{
			return m_nextEntityID++;
		}
	
		std::unique_ptr<RendererSystem> m_renderSystem;

	private:
		uint32_t m_nextEntityID = 1000;

	};

}
