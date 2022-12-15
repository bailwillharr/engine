#pragma once

#include "ecs/mesh_renderer.hpp"

#include "log.hpp"

#include <cstdint>

namespace engine {

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
	
		std::unique_ptr<ecs::RendererSystem> m_renderSystem;

	private:
		uint32_t m_nextEntityID = 1000;

	};

}
