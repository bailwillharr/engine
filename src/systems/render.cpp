#include "systems/render.h"

#include "application.h"
#include "window.h"
#include "gfx_device.h"

#include "resources/material.h"
#include "resources/shader.h"
#include "resources/texture.h"

#include "resources/mesh.h"

#include <glm/mat4x4.hpp>

namespace engine {

	RenderSystem::RenderSystem(Scene* scene)
		: System(scene, { typeid(TransformComponent).hash_code(), typeid(RenderableComponent).hash_code() }),
		  gfx_(scene_->app()->gfxdev())
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
		const auto cameraTransform = scene_->GetComponent<TransformComponent>(camera_.cam_entity);

		// do not render if camera is not set
		if (cameraTransform == nullptr) return;

		glm::mat4 viewMatrix = glm::inverse(cameraTransform->world_matrix);

		if (scene_->app()->window()->GetWindowResized()) {
			uint32_t w, h;
			gfx_->GetViewportSize(&w, &h);
			viewport_aspect_ratio_ = (float)w / (float)h;
			const float verticalFovRadians = glm::radians(camera_.vertical_fov_degrees);
			const glm::mat4 projMatrix = glm::perspectiveZO(verticalFovRadians, viewport_aspect_ratio_, camera_.clip_near, camera_.clip_far);
			/* update SET 0 */
			RenderData::GlobalSetUniformBuffer globalSetUniformBuffer{
				.proj = projMatrix
			};
			gfx_->WriteUniformBuffer(renderData.global_set_uniform_buffer, 0, sizeof(RenderData::GlobalSetUniformBuffer), &globalSetUniformBuffer);
		}

		RenderData::FrameSetUniformBuffer frameSetUniformBuffer{
			.view = viewMatrix
		};
		gfx_->WriteUniformBuffer(renderData.frame_set_uniform_buffer, 0, sizeof(RenderData::FrameSetUniformBuffer), &frameSetUniformBuffer);

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
			assert(r->material->texture_ != nullptr);
			assert(r->mesh != nullptr);
			if (r->shown == false) continue;

			auto t = scene_->GetComponent<TransformComponent>(entity);
			assert(t != nullptr);

			const gfx::Pipeline* pipeline = r->material->GetShader()->GetPipeline();
			DrawCallData data{};
			data.vb = r->mesh->GetVB();
			data.ib = r->mesh->GetIB();
			data.materialSet = r->material->texture_->GetDescriptorSet();
			data.indexCount = r->mesh->GetCount();
			data.pushConsts.model = t->world_matrix;

			pipelineDrawCalls[pipeline].push_back(data);

		}

		/* begin rendering */
		renderData.draw_buffer = gfx_->BeginRender();

		/* these descriptor set bindings should persist across pipeline changes */
		const gfx::Pipeline* firstPipeline = pipelineDrawCalls.begin()->first;
		gfx_->CmdBindDescriptorSet(renderData.draw_buffer, firstPipeline, renderData.global_set, 0);
		gfx_->CmdBindDescriptorSet(renderData.draw_buffer, firstPipeline, renderData.frame_set, 1);

		for (const auto& [pipeline, drawCalls] : pipelineDrawCalls) {
			gfx_->CmdBindPipeline(renderData.draw_buffer, pipeline);
			for (const auto& drawCall : drawCalls) {
				gfx_->CmdBindDescriptorSet(renderData.draw_buffer, pipeline, drawCall.materialSet, 2);
				gfx_->CmdPushConstants(renderData.draw_buffer, pipeline, 0, sizeof(PushConstants), &drawCall.pushConsts);
				gfx_->CmdBindVertexBuffer(renderData.draw_buffer, 0, drawCall.vb);
				gfx_->CmdBindIndexBuffer(renderData.draw_buffer, drawCall.ib);
				gfx_->CmdDrawIndexed(renderData.draw_buffer, drawCall.indexCount, 1, 0, 0, 0);
			}
		}

		/* draw */
		gfx_->FinishRender(renderData.draw_buffer);

	}

	void RenderSystem::SetCameraEntity(uint32_t entity)
	{
		camera_.cam_entity = entity;
	}

}

