#pragma once

#include "log.hpp"

#include "gfx.hpp"
#include "gfx_device.hpp"

#include <glm/mat4x4.hpp>

#include <string>

namespace engine::resources {

class Shader {

public:

	// defines what vertex inputs are defined, position is always vec3
	struct VertexParams {
		bool hasNormal;
		bool hasUV0;
		bool hasUV1;
		bool hasUV2;
		bool hasUV3;
		bool hasTangent;
		bool hasColor;
	};

	Shader(GFXDevice* gfx, const char* vertPath, const char* fragPath, const VertexParams& vertexParams, bool alphaBlending, bool cullBackFace)
		: m_gfx(gfx)
	{
		int index = 0;
		uint32_t stride = 0;
		gfx::VertexFormat vertFormat{};

		vertFormat.attributeDescriptions.emplace_back(index++, gfx::VertexAttribFormat::VEC3, stride);
		stride += 3 * sizeof(float);

		if (vertexParams.hasNormal) {
			vertFormat.attributeDescriptions.emplace_back(index++, gfx::VertexAttribFormat::VEC3, stride);
			stride += 3 * sizeof(float);
		}
		if (vertexParams.hasUV0) {
			vertFormat.attributeDescriptions.emplace_back(index++, gfx::VertexAttribFormat::VEC2, stride);
			stride += 2 * sizeof(float);
		}
		if (vertexParams.hasUV1) {
			vertFormat.attributeDescriptions.emplace_back(index++, gfx::VertexAttribFormat::VEC2, stride);
			stride += 2 * sizeof(float);
		}
		if (vertexParams.hasUV2) {
			vertFormat.attributeDescriptions.emplace_back(index++, gfx::VertexAttribFormat::VEC2, stride);
			stride += 2 * sizeof(float);
		}
		if (vertexParams.hasUV3) {
			vertFormat.attributeDescriptions.emplace_back(index++, gfx::VertexAttribFormat::VEC2, stride);
			stride += 2 * sizeof(float);
		}
		if (vertexParams.hasTangent) {
			vertFormat.attributeDescriptions.emplace_back(index++, gfx::VertexAttribFormat::VEC3, stride);
			stride += 3 * sizeof(float);
		}
		if (vertexParams.hasColor) {
			vertFormat.attributeDescriptions.emplace_back(index++, gfx::VertexAttribFormat::VEC3, stride);
			stride += 3 * sizeof(float);
		}
		vertFormat.stride = stride;

		m_pipeline = m_gfx->createPipeline(vertPath, fragPath, vertFormat, sizeof(glm::mat4), alphaBlending, cullBackFace);

		INFO("Loaded shader: {}, vertex attribs: {}", vertPath, vertFormat.attributeDescriptions.size());
	}
	~Shader()
	{
		m_gfx->destroyPipeline(m_pipeline);
	}
	Shader(const Shader&) = delete;
	Shader& operator=(const Shader&) = delete;

	const gfx::Pipeline* getPipeline() { return m_pipeline; }

private:
	GFXDevice* const m_gfx;
	const gfx::Pipeline* m_pipeline;

};

}
