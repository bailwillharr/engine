#include "resources/shader.hpp"

#include "application.hpp"
#include "gfx_device.hpp"
#include "log.hpp"

#include <glm/mat4x4.hpp>

namespace engine::resources {


	Shader::Shader(RenderData* renderData, const char* vertPath, const char* fragPath, const VertexParams& vertexParams, bool alphaBlending, bool cullBackFace)
		: m_gfx(renderData->gfxdev.get())
	{

		uint32_t index = 0;
		uint32_t stride = 0;
		gfx::VertexFormat vertFormat{};

		vertFormat.attribute_descriptions.emplace_back(index++, gfx::VertexAttribFormat::kFloat3, stride);
		stride += 3 * sizeof(float);

		if (vertexParams.hasNormal) {
			vertFormat.attribute_descriptions.emplace_back(index++, gfx::VertexAttribFormat::kFloat3, stride);
			stride += 3 * sizeof(float);
		}
		if (vertexParams.hasTangent) {
			vertFormat.attribute_descriptions.emplace_back(index++, gfx::VertexAttribFormat::kFloat4, stride);
			stride += 4 * sizeof(float);
		}
		if (vertexParams.hasColor) {
			vertFormat.attribute_descriptions.emplace_back(index++, gfx::VertexAttribFormat::kFloat4, stride);
			stride += 4 * sizeof(float);
		}
		if (vertexParams.hasUV0) {
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

		m_pipeline = m_gfx->CreatePipeline(info);

		LOG_INFO("Loaded shader: {}, vertex attribs: {}", vertPath, vertFormat.attribute_descriptions.size());

	}

	Shader::~Shader()
	{
		m_gfx->DestroyPipeline(m_pipeline);
	}

	const gfx::Pipeline* Shader::getPipeline()
	{
		return m_pipeline;
	}

}
