#pragma once

#include "engine_api.h"

#include "resource.hpp"

#include <glm/mat4x4.hpp>

#include <string>
#include <map>

namespace engine::gfx {
	class Pipeline;
}

namespace engine::resources {

class ENGINE_API Shader : public Resource {

public:
	Shader(const std::filesystem::path& resPath);
	~Shader() override;

	struct UniformBuffer {
		glm::mat4 transform;
	};

	gfx::Pipeline* getPipeline()
	{
		return m_pipeline;
	}

private:
	gfx::Pipeline* m_pipeline = nullptr;

};

}
