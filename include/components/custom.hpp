#pragma once

#include "component.hpp"

#include <glm/mat4x4.hpp>

namespace components {

	class CustomComponent : public Component {

	public:
		CustomComponent(Object* parent);
		virtual ~CustomComponent() = 0;

		virtual void onInit();
		virtual void onUpdate(glm::mat4 t);

	};

}