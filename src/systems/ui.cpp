#include "systems/ui.h"

#include "components/ui_element.h"

#include <typeinfo>

namespace engine {

	UISystem::UISystem(Scene* scene)
		: System(scene, { typeid(UIElementComponent).hash_code() })
	{

	}

	void UISystem::OnUpdate(float ts)
	{
		(void)ts;

	}

}
