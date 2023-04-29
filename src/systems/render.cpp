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
		  m_gfx(scene_->app()->gfxdev())
	{
	}

	RenderSystem::~RenderSystem()
	{
	}

	void RenderSystem::OnUpdate(float ts)
	{
		(void)ts;
		
		RenderData& renderData = scene_->app()->render_data_;
		
		/* camera stuff */
		const auto cameraTransform = scene_->GetComponent<TransformComponent>(m_camera.camEntity);

		// do not render if camera is not set
		if (cameraTransform == nullptr) return;

		glm::mat4 viewMatrix = glm::inverse(cameraTransform->worldMatrix);

		if (scene_->app()->window()->getWindowResized()) {
			uint32_t w, h;
			m_gfx->GetViewportSize(&w, &h);
			m_viewportAspectRatio = (float)w / (float)h;
			const float verticalFovRadians = glm::radians(m_camera.verticalFovDegrees);
			const glm::mat4 projMatrix = glm::perspectiveZO(verticalFovRadians, m_viewportAspectRatio, m_camera.clipNear, m_camera.clipFar);
			/* update SET 0 */
			RenderData::GlobalSetUniformBuffer globalSetUniformBuffer{
				.proj = projMatrix
			};
			m_gfx->WriteUniformBuffer(renderData.global_set_uniform_buffer, 0, sizeof(RenderData::GlobalSetUniformBuffer), &globalSetUniformBuffer);
		}

		RenderData::FrameSetUniformBuffer frameSetUniformBuffer{
			.view = viewMatrix
		};
		m_gfx->WriteUniformBuffer(renderData.frame_set_uniform_buffer, 0, sizeof(RenderData::FrameSetUniformBuffer), &frameSetUniformBuffer);

		/* render all renderable entities */

		struct PushConstants {
			glm::mat4 model;
		};

		struct DrawCallData {
			const gfx::Buffer* vb;
			const gfx::Buffer* ib;
			const gfx::DescriptorSet* materialSet;
			uint32_t indexCount;
			PushConstants pushConsts;
		};
		std::unordered_map <const gfx::Pipeline*, std::vector<DrawCallData>> pipelineDrawCalls{};

		for (uint32_t entity : entities_) {

			auto r = scene_->GetComponent<RenderableComponent>(entity);
			assert(r != nullptr);
			assert(r->material != nullptr);
			assert(r->material->m_texture != nullptr);
			assert(r->mesh != nullptr);
			if (r->shown == false) continue;

			auto t = scene_->GetComponent<TransformComponent>(entity);
			assert(t != nullptr);

			const gfx::Pipeline* pipeline = r->material->getShader()->getPipeline();
			DrawCallData data{};
			data.vb = r->mesh->getVB();
			data.ib = r->mesh->getIB();
			data.materialSet = r->material->m_texture->getDescriptorSet();
			data.indexCount = r->mesh->getCount();
			data.pushConsts.model = t->worldMatrix;

			pipelineDrawCalls[pipeline].push_back(data);

		}

		/* begin rendering */
		renderData.draw_buffer = m_gfx->BeginRender();

		/* these descriptor set bindings should persist across pipeline changes */
		const gfx::Pipeline* firstPipeline = pipelineDrawCalls.begin()->first;
		m_gfx->CmdBindDescriptorSet(renderData.draw_buffer, firstPipeline, renderData.global_set, 0);
		m_gfx->CmdBindDescriptorSet(renderData.draw_buffer, firstPipeline, renderData.frame_set, 1);

		for (const auto& [pipeline, drawCalls] : pipelineDrawCalls) {
			m_gfx->CmdBindPipeline(renderData.draw_buffer, pipeline);
			for (const auto& drawCall : drawCalls) {
				m_gfx->CmdBindDescriptorSet(renderData.draw_buffer, pipeline, drawCall.materialSet, 2);
				m_gfx->CmdPushConstants(renderData.draw_buffer, pipeline, 0, sizeof(PushConstants), &drawCall.pushConsts);
				m_gfx->CmdBindVertexBuffer(renderData.draw_buffer, 0, drawCall.vb);
				m_gfx->CmdBindIndexBuffer(renderData.draw_buffer, drawCall.ib);
				m_gfx->CmdDrawIndexed(renderData.draw_buffer, drawCall.indexCount, 1, 0, 0, 0);
			}
		}

		/* draw */
		m_gfx->FinishRender(renderData.draw_buffer);

	}

	void RenderSystem::setCameraEntity(uint32_t entity)
	{
		m_camera.camEntity = entity;
	}

}

