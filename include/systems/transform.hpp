#pragma once

#include "ecs_system.hpp"

namespace engine {

	class TransformSystem : public System {

	public:
		TransformSystem(Scene* scene);

		void OnUpdate(float ts) override;

		uint32_t getChildEntity(uint32_t parent, const std::string& tag);

	};

}

