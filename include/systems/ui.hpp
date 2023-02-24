#pragma once

#include "ecs_system.hpp"

namespace engine {

	class UISystem : public System {

	public:
		UISystem(Scene* scene);

		void onUpdate(float ts) override;

	private:

	};

}

