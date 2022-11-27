#include "resources/shader.hpp"

#include "log.hpp"

#include "gfx_device.hpp"

#include <vector>

namespace engine::resources {

Shader::Shader(const std::filesystem::path& resPath) : Resource(resPath, "shader")
{

	gfx::VertexFormat vertexFormat {};
	vertexFormat.stride = 8 * sizeof(float);
	vertexFormat.attributeDescriptions.emplace_back(0, gfx::VertexAttribFormat::VEC3, 0); // pos
	vertexFormat.attributeDescriptions.emplace_back(1, gfx::VertexAttribFormat::VEC3, sizeof(glm::vec3)); // norm
	vertexFormat.attributeDescriptions.emplace_back(2, gfx::VertexAttribFormat::VEC2, sizeof(glm::vec3) + sizeof(glm::vec3)); // uv

	const std::string vertexShaderPath = (resPath.parent_path()/std::filesystem::path(resPath.stem().string() + ".vert")).string();
	const std::string fragmentShaderPath = (resPath.parent_path()/std::filesystem::path(resPath.stem().string() + ".frag")).string();

	m_pipeline = gfxdev->createPipeline(vertexShaderPath.c_str(), fragmentShaderPath.c_str(), vertexFormat, sizeof(UniformBuffer), true, true);

}

Shader::~Shader()
{
	gfxdev->destroyPipeline(m_pipeline);
}

}
