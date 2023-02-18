#include "resources/shader.hpp"

#include "gfx_device.hpp"
#include "log.hpp"

#include <glm/mat4x4.hpp>

namespace engine::resources {


	Shader::Shader(GFXDevice* gfx, const char* vertPath, const char* fragPath, const VertexParams& vertexParams, bool alphaBlending, bool cullBackFace)
		: m_gfx(gfx)
	{
		uint32_t index = 0;
		uint32_t stride = 0;
		gfx::VertexFormat vertFormat{};

		vertFormat.attributeDescriptions.emplace_back(index++, gfx::VertexAttribFormat::FLOAT3, stride);
		stride += 3 * sizeof(float);

		if (vertexParams.hasNormal) {
			vertFormat.attributeDescriptions.emplace_back(index++, gfx::VertexAttribFormat::FLOAT3, stride);
			stride += 3 * sizeof(float);
		}
		if (vertexParams.hasTangent) {
			vertFormat.attributeDescriptions.emplace_back(index++, gfx::VertexAttribFormat::FLOAT4, stride);
			stride += 4 * sizeof(float);
		}
		if (vertexParams.hasColor) {
			vertFormat.attributeDescriptions.emplace_back(index++, gfx::VertexAttribFormat::FLOAT4, stride);
			stride += 4 * sizeof(float);
		}
		if (vertexParams.hasUV0) {
			vertFormat.attributeDescriptions.emplace_back(index++, gfx::VertexAttribFormat::FLOAT2, stride);
			stride += 2 * sizeof(float);
		}
		vertFormat.stride = stride;

		m_pipeline = m_gfx->createPipeline(vertPath, fragPath, vertFormat, sizeof(glm::mat4) * 2, alphaBlending, cullBackFace);

		INFO("Loaded shader: {}, vertex attribs: {}", vertPath, vertFormat.attributeDescriptions.size());
	}

	Shader::~Shader()
	{
		m_gfx->destroyPipeline(m_pipeline);
	}

	const gfx::Pipeline* Shader::getPipeline()
	{
		return m_pipeline;
	}

}
