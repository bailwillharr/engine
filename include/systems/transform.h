#ifndef ENGINE_INCLUDE_SYSTEMS_TRANSFORM_H_
#define ENGINE_INCLUDE_SYSTEMS_TRANSFORM_H_

#include "ecs_system.h"

namespace engine {

	class TransformSystem : public System {

	public:
		TransformSystem(Scene* scene);

		void OnUpdate(float ts) override;

		uint32_t GetChildEntity(uint32_t parent, const std::string& tag);

	};

}

#endif