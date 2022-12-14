#pragma once

#include <cstdint>

namespace engine {

	class Scene {
	
	public:
		Scene();
		Scene(const Scene&) = delete;
		Scene& operator=(const Scene&) = delete;
		~Scene();

		uint32_t createEntity()
		{
			return m_nextEntityID++;
		}

	private:
		uint32_t m_nextEntityID = 1000;

	};

}
