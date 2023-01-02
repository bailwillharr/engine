#include "ecs_system.hpp"

#include "scene.hpp"

namespace engine {

	System::System(Scene* scene, std::set<size_t> requiredComponentHashes)
		: m_scene(scene)
	{
		for (size_t componentHash : requiredComponentHashes) {
			size_t componentSignaturePosition = m_scene->getComponentSignaturePosition(componentHash);
			m_signature.set(componentSignaturePosition);
		}
	}

}
