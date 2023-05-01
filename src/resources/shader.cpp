#include "resources/shader.h"

#include "application.h"
#include "gfx_device.h"
#include "log.h"

#include <glm/mat4x4.hpp>

namespace engine::resources {


	Shader::Shader(RenderData* renderData, const char* vertPath, const char* fragPath, const VertexParams& vertexParams, bool alphaBlending, bool cullBackFace)
		: gfx_(renderData->gfxdev.get())
	{

		uint32_t index = 0;
		uint32_t stride = 0;
		gfx::VertexFormat vertFormat{};

		vertFormat.attribute_descriptions.emplace_back(index++, gfx::VertexAttribFormat::kFloat3, stride);
		stride += 3 * sizeof(float);

		if (vertexParams.has_normal) {
			vertFormat.attribute_descriptions.emplace_back(index++, gfx::VertexAttribFormat::kFloat3, stride);
			stride += 3 * sizeof(float);
		}
		if (vertexParams.has_tangent) {
			vertFormat.attribute_descriptions.emplace_back(index++, gfx::VertexAttribFormat::kFloat4, stride);
			stride += 4 * sizeof(float);
		}
		if (vertexParams.has_color) {
			vertFormat.attribute_descriptions.emplace_back(index++, gfx::VertexAttribFormat::kFloat4, stride);
			stride += 4 * sizeof(float);
		}
		if (vertexParams.has_uv0) {
			vertFormat.attribute_descriptions.emplace_back(index++, gfx::VertexAttribFormat::kFloat2, stride);
			stride += 2 * sizeof(float);
		}
		vertFormat.stride = stride;

		gfx::PipelineInfo info{};
		info.vert_shader_path = vertPath;
		info.frag_shader_path = fragPath;
		info.vertex_format = vertFormat;
		info.alpha_blending = alphaBlending;
		info.backface_culling = cullBackFace;
		info.descriptor_set_layouts.push_back(renderData->global_set_layout);
		info.descriptor_set_layouts.push_back(renderData->frame_set_layout);
		info.descriptor_set_layouts.push_back(renderData->material_set_layout);

		pipeline_ = gfx_->CreatePipeline(info);

		LOG_INFO("Loaded shader: {}, vertex attribs: {}", vertPath, vertFormat.attribute_descriptions.size());

	}

	Shader::~Shader()
	{
		gfx_->DestroyPipeline(pipeline_);
	}

	const gfx::Pipeline* Shader::GetPipeline()
	{
		return pipeline_;
	}

}
