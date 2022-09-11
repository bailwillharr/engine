#pragma once

#include "engine_api.h"

#include "component.hpp"

#include <glm/mat4x4.hpp>

namespace components {

	class ENGINE_API CustomComponent : public Component {

	public:
		CustomComponent(Object* parent);
		virtual ~CustomComponent() = 0;

		virtual void onUpdate(glm::mat4 t) {}

	};

}