#pragma once

#include <map>
#include <memory>

namespace engine::ecs {

	template <class T>
	class System {
	
	public:
		std::map<uint32_t, T> m_components{};

		virtual void onUpdate(float ts) = 0;

	};

}
