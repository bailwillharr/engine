#pragma once

#include "log.hpp"

#include <cstdint>

namespace engine {

	class Application;

	namespace ecs {
		class RendererSystem;
	}

	class Scene {
	
	public:
		Scene(Application* app);
		Scene(const Scene&) = delete;
		Scene& operator=(const Scene&) = delete;
		~Scene();

		void update(float ts);

		uint32_t createEntity()
		{
			return m_nextEntityID++;
		}
	
		std::unique_ptr<ecs::RendererSystem> m_renderSystem;

		Application* app() { return m_app; }

	private:
		Application* const m_app;
		uint32_t m_nextEntityID = 1000;

	};

}
