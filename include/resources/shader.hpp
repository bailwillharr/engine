#pragma once

#include "gfx.hpp"

namespace engine {
	class GFXDevice;
}

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

	Shader(GFXDevice* gfx, const char* vertPath, const char* fragPath, const VertexParams& vertexParams, bool alphaBlending, bool cullBackFace);
	~Shader();
	Shader(const Shader&) = delete;
	Shader& operator=(const Shader&) = delete;

	const gfx::Pipeline* getPipeline();

private:
	GFXDevice* const m_gfx;
	const gfx::Pipeline* m_pipeline;

};

}
