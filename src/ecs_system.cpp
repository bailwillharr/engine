#include "ecs_system.h"

#include "scene.h"

namespace engine {

	System::System(Scene* scene, std::set<size_t> requiredComponentHashes)
		: scene_(scene)
	{
		for (size_t componentHash : requiredComponentHashes) {
			size_t componentSignaturePosition = scene_->GetComponentSignaturePosition(componentHash);
			signature_.set(componentSignaturePosition);
		}
	}

}
