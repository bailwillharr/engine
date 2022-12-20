#pragma once

#include "ecs_system.hpp"

#include "resources/material.hpp"
#include "resources/shader.hpp"
#include "resources/mesh.hpp"
#include "log.hpp"

#include "scene.hpp"
#include "application.hpp"
#include "gfx_device.hpp"

#include <glm/mat4x4.hpp>

namespace engine::ecs {

	struct MeshRendererComponent {
		std::shared_ptr<resources::Material> material;
		std::shared_ptr<resources::Mesh> mesh;
	};

	class RendererSystem : public EcsSystem<MeshRendererComponent> {

	public:
		RendererSystem(Scene* scene)
			: EcsSystem<MeshRendererComponent>(scene)
		{

		}

		void onUpdate(float ts) override
		{
			for (const auto& [id, data] : m_components) {
				DEBUG("rendering entity {}\tts={}", id, ts);
				DEBUG("material shader addr: {}", (void*)data.material->getShader());

				assert(data.material != nullptr);
				assert(data.mesh != nullptr);

				struct {
					glm::mat4 model;
					glm::mat4 view;
				} pushConsts{};
				pushConsts.model = glm::mat4{1.0f};
				pushConsts.view = glm::mat4{1.0f};

				m_scene->app()->gfx()->draw(
						data.material->getShader()->getPipeline(),
						data.mesh->getVB(),
						data.mesh->getIB(),
						data.mesh->getCount(),
						&pushConsts, sizeof(pushConsts), nullptr);
			}
		}
	};



}
