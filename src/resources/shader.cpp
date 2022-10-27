#include "resources/shader.hpp"

#include "log.hpp"

#include "gfx_device.hpp"

#include <string>
#include <fstream>
#include <vector>
#include <span>

static std::unique_ptr<std::vector<char>> readFile(const char * path)
{
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	//file.exceptions(std::ifstream::failbit); // throw exception if file cannot be opened
	if (file.fail()) {
		throw std::runtime_error("Failed to open file for reading: " + std::string(path));
	}
	size_t size = file.tellg();
	file.seekg(0, std::ios::beg);
	auto buf = std::make_unique<std::vector<char>>();
	buf->resize(size);
	file.rdbuf()->sgetn(buf->data(), size);
	return buf;
}

namespace engine::resources {

Shader::Shader(const std::filesystem::path& resPath) : Resource(resPath, "shader")
{

	gfx::VertexFormat vertexFormat {};
	vertexFormat.attributeDescriptions.emplace_back(0, gfx::VertexAttribFormat::VEC3, 0); // pos
	vertexFormat.attributeDescriptions.emplace_back(1, gfx::VertexAttribFormat::VEC3, sizeof(glm::vec3)); // norm
	vertexFormat.attributeDescriptions.emplace_back(2, gfx::VertexAttribFormat::VEC2, sizeof(glm::vec3) + sizeof(glm::vec3)); // uv

	const std::string vertexShaderPath = (resPath.parent_path()/std::filesystem::path(resPath.stem().string() + ".vert.spv")).string();
	const std::string fragmentShaderPath = (resPath.parent_path()/std::filesystem::path(resPath.stem().string() + ".frag.spv")).string();

	m_pipeline = gfxdev->createPipeline(vertexShaderPath.c_str(), fragmentShaderPath.c_str(), vertexFormat, sizeof(UniformBuffer));

}

Shader::~Shader()
{
	gfxdev->destroyPipeline(m_pipeline);
}

}
