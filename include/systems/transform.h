#ifndef ENGINE_INCLUDE_SYSTEMS_TRANSFORM_H_
#define ENGINE_INCLUDE_SYSTEMS_TRANSFORM_H_

#include "ecs.h"

namespace engine {

	class TransformSystem : public System {

	public:
		TransformSystem(Scene* scene);

		void OnUpdate(float ts) override;

		Entity GetChildEntity(Entity parent, const std::string& tag);

	};

}

#endif