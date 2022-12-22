#pragma once

#include "ecs_system.hpp"

#include "resources/material.hpp"
#include "resources/shader.hpp"
#include "resources/texture.hpp"
#include "resources/mesh.hpp"
#include "log.hpp"

#include "scene.hpp"
#include "application.hpp"
#include "window.hpp"
#include "gfx_device.hpp"

#include <glm/mat4x4.hpp>
#include <glm/gtc/quaternion.hpp>

#include <set>

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
			updateProjectionMatrix();
		}

		void drawMeshes(const std::map<uint32_t, glm::mat4>& worldTransforms)
		{

			glm::mat4 cameraTransform{1.0f};
//			cameraTransform = glm::mat4_cast(m_cameraRot);
//			reinterpret_cast<glm::vec3&>(cameraTransform[3]) = m_cameraPos;
			glm::mat4 viewMatrix = glm::inverse(cameraTransform);

			if (m_scene->app()->window()->getWindowResized()) {
				updateProjectionMatrix();
			}

			std::set<resources::Shader*> uniqueShaders{};
			for (const auto& [id, data] : m_components) {
				uniqueShaders.insert(data.material->getShader());
			}
			for (resources::Shader* shader : uniqueShaders) {
				DEBUG("TEST {}", (void*)shader);
				m_scene->app()->gfx()->updateUniformBuffer(shader->getPipeline(), &m_projMatrix, sizeof(m_projMatrix), 0);
			}

			for (const auto& [id, data] : m_components) {
				DEBUG("drawing entity {}", id);
				DEBUG("material shader addr: {}", (void*)data.material->getShader());

				assert(data.material != nullptr);
				assert(data.mesh != nullptr);

				struct {
					glm::mat4 model;
					glm::mat4 view;
				} pushConsts{};
				pushConsts.model = worldTransforms.at(id);
				pushConsts.view = glm::mat4{1.0f};

				m_scene->app()->gfx()->draw(
						data.material->getShader()->getPipeline(),
						data.mesh->getVB(),
						data.mesh->getIB(),
						data.mesh->getCount(),
						&pushConsts, sizeof(pushConsts),
						data.material->m_texture->getHandle());
			}
		}

		glm::vec3 m_cameraPos;
		glm::quat m_cameraRot;

		glm::mat4 m_projMatrix;

	private:

		void updateProjectionMatrix()
		{
			constexpr float NEAR = 0.1f;
			constexpr float FAR = 1000.0f;
			float fovRad = glm::radians(75.0f);
			uint32_t viewportWidth, viewportHeight;
			m_scene->app()->gfx()->getViewportSize(&viewportWidth, &viewportHeight);
			float aspect = (float)viewportWidth / (float)viewportHeight;
			float fovY = fovRad / aspect;
			m_projMatrix = glm::perspectiveZO(fovY, aspect, NEAR, FAR);
		}

	};



}
