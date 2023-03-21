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
			const float verticalFovRadians = glm::radians(m_camera.verticalFovDegrees);
			const glm::mat4 projMatrix = glm::perspectiveZO(verticalFovRadians, m_viewportAspectRatio, m_camera.clipNear, m_camera.clipFar);
			/* update SET 0 */
			RenderData::SetZeroBuffer setZeroBuffer{
				.proj = projMatrix
			};
			m_gfx->writeUniformBuffer(renderData.setZeroBuffer, 0, sizeof(RenderData::SetZeroBuffer), &setZeroBuffer);
		}

		RenderData::SetOneBuffer setOneBuffer{
			.view = viewMatrix
		};
		m_gfx->writeUniformBuffer(renderData.setOneBuffer, 0, sizeof(RenderData::SetOneBuffer), &setOneBuffer);

		/* render all renderable entities */

		struct PushConstants {
			glm::mat4 model;
		};

		struct DrawCallData {
			const gfx::Buffer* vb;
			const gfx::Buffer* ib;
			uint32_t indexCount;
			PushConstants pushConsts;
		};
		std::unordered_map <const gfx::Pipeline*, std::vector<DrawCallData>> pipelineDrawCalls{};

		for (uint32_t entity : m_entities) {

			auto r = m_scene->getComponent<RenderableComponent>(entity);
			assert(r != nullptr);
			assert(r->material != nullptr);
			assert(r->mesh != nullptr);
			if (r->shown == false) continue;

			auto t = m_scene->getComponent<TransformComponent>(entity);
			assert(t != nullptr);

			const gfx::Pipeline* pipeline = r->material->getShader()->getPipeline();
			DrawCallData data{};
			data.vb = r->mesh->getVB();
			data.ib = r->mesh->getIB();
			data.indexCount = r->mesh->getCount();
			data.pushConsts.model = t->worldMatrix;

			pipelineDrawCalls[pipeline].push_back(data);

		}

		/* begin rendering */
		renderData.drawBuffer = m_gfx->beginRender();

		/* these descriptor set bindings should persist across pipeline changes */
		const gfx::Pipeline* firstPipeline = pipelineDrawCalls.begin()->first;
		m_gfx->cmdBindDescriptorSet(renderData.drawBuffer, firstPipeline, renderData.setZero, 0);
		m_gfx->cmdBindDescriptorSet(renderData.drawBuffer, firstPipeline, renderData.setOne, 1);

		for (const auto& [pipeline, drawCalls] : pipelineDrawCalls) {
			m_gfx->cmdBindPipeline(renderData.drawBuffer, pipeline);
			for (const auto& drawCall : drawCalls) {
				m_gfx->cmdPushConstants(renderData.drawBuffer, pipeline, 0, sizeof(PushConstants), &drawCall.pushConsts);
				m_gfx->cmdBindVertexBuffer(renderData.drawBuffer, 0, drawCall.vb);
				m_gfx->cmdBindIndexBuffer(renderData.drawBuffer, drawCall.ib);
				m_gfx->cmdDrawIndexed(renderData.drawBuffer, drawCall.indexCount, 1, 0, 0, 0);
			}
		}

		/* draw */
		m_gfx->finishRender(renderData.drawBuffer);

	}

	void RenderSystem::setCameraEntity(uint32_t entity)
	{
		m_camera.camEntity = entity;
	}

}

