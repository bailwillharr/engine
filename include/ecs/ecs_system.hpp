#pragma once

#include <map>
#include <memory>

namespace engine {

	class Scene;

	template <class T>
	class EcsSystem {
	
	public:
		EcsSystem(Scene* scene)
			: m_scene(scene)
		{

		}
		~EcsSystem() {}
		EcsSystem(const EcsSystem&) = delete;
		EcsSystem& operator=(const EcsSystem&) = delete;

		Scene* const m_scene;

		std::map<uint32_t, T> m_components{};

	};

}
