#include "systems/render2d.h"

#include <typeinfo>

#include "components/transform.h"

namespace engine {

	Render2DSystem::Render2DSystem(Scene* scene)
		: System(scene, { typeid(TransformComponent).hash_code() })
	{
	}

	Render2DSystem::~Render2DSystem()
	{
	}

	void Render2DSystem::OnUpdate(float ts)
	{
		(void)ts;
	}

}

