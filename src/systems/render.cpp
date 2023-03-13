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
		: System(scene, { typeid(TransformComponent).hash_code(), typeid(RenderableComponent).hash_code() }),
		  m_gfx(m_scene->app()->gfx())
	{
	}

	RenderSystem::~RenderSystem()
	{
	}

	void RenderSystem::onUpdate(float ts)
	{
		(void)ts;
		
		RenderData& renderData = m_scene->app()->renderData;
		
		/* camera stuff */
		const auto cameraTransform = m_scene->getComponent<TransformComponent>(m_camera.camEntity);

		// do not render if camera is not set
		if (cameraTransform == nullptr) return;

		glm::mat4 viewMatrix = glm::inverse(cameraTransform->worldMatrix);

		if (m_scene->app()->window()->getWindowResized()) {
			uint32_t w, h;
			m_gfx->getViewportSize(&w, &h);
			m_viewportAspectRatio = (float)w / (float)h;
		}
		const float verticalFovRadians = glm::radians(m_camera.verticalFovDegrees);
		const glm::mat4 projMatrix = glm::perspectiveZO(verticalFovRadians, m_viewportAspectRatio, m_camera.clipNear, m_camera.clipFar);

		/* update SET 0 */
		RenderData::SetZeroBuffer uniform{};
		uniform.proj = projMatrix;
		uniform.myValue.x = 1.0f;
		m_value = glm::mod(m_value + ts, 1.0f);
		uniform.myValue.y = m_value;
		m_gfx->writeDescriptorBuffer(renderData.setZeroBuffer, 0, sizeof(RenderData::SetZeroBuffer), &uniform);

		/* render all renderable entities */

		for (uint32_t entity : m_entities) {

			auto r = m_scene->getComponent<RenderableComponent>(entity);
			if (r->shown == false) continue;

			auto t = m_scene->getComponent<TransformComponent>(entity);

			assert(r->material != nullptr);
			assert(r->mesh != nullptr);
			//assert(r->material->m_texture != nullptr);

			struct {
				glm::mat4 model;
				glm::mat4 view;
			} pushConsts{};

			pushConsts.model = t->worldMatrix;
			pushConsts.view = viewMatrix;

			m_gfx->cmdBindPipeline(renderData.drawBuffer, r->material->getShader()->getPipeline());
			m_gfx->cmdBindDescriptorSet(renderData.drawBuffer, r->material->getShader()->getPipeline(), renderData.setZero, 0);
			m_gfx->cmdPushConstants(renderData.drawBuffer, r->material->getShader()->getPipeline(), 0, sizeof(pushConsts), &pushConsts);
			m_gfx->cmdBindVertexBuffer(renderData.drawBuffer, 0, r->mesh->getVB());
			m_gfx->cmdBindIndexBuffer(renderData.drawBuffer, r->mesh->getIB());
			m_gfx->cmdDrawIndexed(renderData.drawBuffer, r->mesh->getCount(), 1, 0, 0, 0);

		}

	}

	void RenderSystem::setCameraEntity(uint32_t entity)
	{
		m_camera.camEntity = entity;
	}

}

