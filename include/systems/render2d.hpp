#pragma once

#include "ecs_system.hpp"

namespace engine {

	class Render2DSystem : public System {

	public:
		Render2DSystem(Scene* scene);
		~Render2DSystem();

		void OnUpdate(float ts) override;

	private:

	};

}

