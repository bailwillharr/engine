#include "systems/ui.hpp"

#include "components/ui_element.hpp"

#include <typeinfo>

namespace engine {

	UISystem::UISystem(Scene* scene)
		: System(scene, { typeid(UIElementComponent).hash_code() })
	{

	}

	void UISystem::onUpdate(float ts)
	{
		(void)ts;

	}

}
