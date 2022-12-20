#pragma once

#include "gfx.hpp"
#include "gfx_device.hpp"

#include <glm/mat4x4.hpp>

#include <string>

namespace engine::resources {

class Shader {

public:
	Shader(GFXDevice* gfx, const char* vertPath, const char* fragPath, const gfx::VertexFormat& vertFormat, bool alphaBlending, bool cullBackFace)
		: m_gfx(gfx)
	{
		m_pipeline = m_gfx->createPipeline(vertPath, fragPath, vertFormat, sizeof(glm::mat4), alphaBlending, cullBackFace);
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
