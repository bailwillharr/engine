#pragma once

#include "ecs_system.hpp"

namespace engine {

	class PhysicsSystem : public System {

	public:
		PhysicsSystem(Scene* scene);

		void onUpdate(float ts) override;

	};

}

