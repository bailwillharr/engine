#pragma once

#include "ecs_system.hpp"

#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>

namespace engine::ecs {

	struct TransformComponent {
		uint32_t parent;
		glm::vec3 scale;
		glm::vec3 position;
		glm::quat rotation;
	};

	class TransformSystem : public EcsSystem<TransformComponent> {

	public:
		TransformSystem(Scene* scene)
			: EcsSystem<TransformComponent>(scene)
		{

		}

		std::unique_ptr<std::map<uint32_t, glm::mat4>> getMatrices()
		{
			auto transforms = std::make_unique<std::map<uint32_t, glm::mat4>>();
			for (const auto& [id, data] : m_components) {
				glm::mat4 transform;
				
				// rotation
				transform = glm::mat4_cast(data.rotation);
				// position
				reinterpret_cast<glm::vec3&>(transform[3]) = data.position;
				// scale (effectively applied first)
				transform = glm::scale(transform, data.scale);

				if (data.parent != 0) {
					transform = transforms->at(data.parent) * transform;
				}

				transforms->emplace(id, transform);
			}
			return transforms;
		}
	};

}
