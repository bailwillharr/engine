#include "ecs.h"

#include "scene.h"

namespace engine {

	System::System(Scene* scene, std::set<size_t> requiredComponentHashes)
		: m_scene(scene)
	{
		for (size_t componentHash : requiredComponentHashes) {
			size_t componentSignaturePosition = m_scene->GetComponentSignaturePosition(componentHash);
			m_signature.set(componentSignaturePosition);
		}
	}

}
