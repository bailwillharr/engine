#include "resources/shader.hpp"

#include "gfx_device.hpp"
#include "log.hpp"

#include <glm/mat4x4.hpp>

namespace engine::resources {


	Shader::Shader(GFXDevice* gfx, const char* vertPath, const char* fragPath, const VertexParams& vertexParams, bool alphaBlending, bool cullBackFace)
		: m_gfx(gfx)
	{

		m_setZeroLayout = m_gfx->createDescriptorSetLayout();

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

		gfx::PipelineInfo info{};
		info.vertShaderPath = vertPath;
		info.fragShaderPath = fragPath;
		info.vertexFormat = vertFormat;
		info.alphaBlending = alphaBlending;
		info.backfaceCulling = cullBackFace;
		info.descriptorSetLayouts.push_back(m_setZeroLayout);

		m_pipeline = m_gfx->createPipeline(info);

		LOG_INFO("Loaded shader: {}, vertex attribs: {}", vertPath, vertFormat.attributeDescriptions.size());

		/* allocate uniform descriptor set */
		m_setZero = m_gfx->allocateDescriptorSet(m_setZeroLayout);
		/* fill with data */
		glm::vec4 myValue = { 0.5f, 0.5f, 0.5f, 1.0f };
		m_setZeroBuffer = m_gfx->createBuffer(gfx::BufferType::UNIFORM, sizeof(glm::vec4), &myValue);
		m_gfx->updateDescriptor(m_setZero, 0, m_setZeroBuffer);

	}

	Shader::~Shader()
	{
		m_gfx->destroyBuffer(m_setZeroBuffer);
		m_gfx->destroyPipeline(m_pipeline);
		m_gfx->destroyDescriptorSetLayout(m_setZeroLayout);
	}

	const gfx::Pipeline* Shader::getPipeline()
	{
		return m_pipeline;
	}

}
