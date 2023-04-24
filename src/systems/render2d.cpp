#include "systems/render2d.hpp"

#include "components/transform.hpp"

namespace engine {

	Render2DSystem::Render2DSystem(Scene* scene)
		: System(scene, { typeid(TransformComponent).hash_code() })
	{
	}

	Render2DSystem::~Render2DSystem()
	{
	}

	void Render2DSystem::onUpdate(float ts)
	{
		(void)ts;
	}

}

