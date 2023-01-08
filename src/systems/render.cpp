#include "systems/render.hpp"

#include "application.hpp"
#include "window.hpp"
#include "gfx_device.hpp"

#include "resources/material.hpp"
#include "resources/shader.hpp"
#include "resources/texture.hpp"

#include "resources/mesh.hpp"

#include <glm/mat4x4.hpp>

namespace engine {

	RenderSystem::RenderSystem(Scene* scene)
		: System(scene, { typeid(TransformComponent).hash_code(), typeid(RenderableComponent).hash_code() })
	{

	}

	void RenderSystem::onUpdate(float ts)
	{
		(void)ts;

		GFXDevice* const gfx = m_scene->app()->gfx();

		/* camera stuff */

		const auto cameraTransform = m_scene->getComponent<TransformComponent>(m_camera.camEntity);

		// do not render if camera is not set
		if (cameraTransform == nullptr) return;

		glm::mat4 viewMatrix = glm::inverse(cameraTransform->worldMatrix);

		if (m_scene->app()->window()->getWindowResized()) {
			uint32_t w, h;
			gfx->getViewportSize(&w, &h);
			m_viewportAspectRatio = (float)w / (float)h;
		}
		const float horizontalFovRadians = glm::radians(m_camera.horizontalFovDegrees);
		const float verticalFov = glm::atan( glm::tan(horizontalFovRadians / 2.0f) / m_viewportAspectRatio ) * 2.0f;
		const glm::mat4 projMatrix = glm::perspectiveZO(verticalFov, m_viewportAspectRatio, m_camera.clipNear, m_camera.clipFar);

		/* render all renderable entities */

		for (uint32_t entity : m_entities) {

			auto t = m_scene->getComponent<TransformComponent>(entity);
			auto r = m_scene->getComponent<RenderableComponent>(entity);

			assert(r->material != nullptr);
			assert(r->mesh != nullptr);
			assert(r->material->m_texture != nullptr);

			gfx->updateUniformBuffer(r->material->getShader()->getPipeline(), &projMatrix, sizeof(projMatrix), 0);

			struct {
				glm::mat4 model;
				glm::mat4 view;
			} pushConsts{};

			pushConsts.model = t->worldMatrix;
			pushConsts.view = viewMatrix;

			gfx->draw(
				r->material->getShader()->getPipeline(),
				r->mesh->getVB(),
				r->mesh->getIB(),
				r->mesh->getCount(),
				&pushConsts,
				sizeof(pushConsts),
				r->material->m_texture->getHandle()
			);

		}

	}

	void RenderSystem::setCameraEntity(uint32_t entity)
	{
		{ m_camera.camEntity = entity; }
	}

}

