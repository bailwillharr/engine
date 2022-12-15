#pragma once

#include <map>
#include <memory>

namespace engine {

	template <class T>
	class EcsSystem {
	
	public:
		std::map<uint32_t, T> m_components{};

		virtual void onUpdate(float ts) = 0;

	};

}
