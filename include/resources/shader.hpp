#pragma once

#include "engine_api.h"

#include "resource.hpp"

#include <glad/glad.h>

#include <glm/mat4x4.hpp>

#include <string>
#include <map>

namespace engine::resources {

class ENGINE_API Shader : public Resource {

public:
	Shader(const std::filesystem::path& resPath);
	~Shader() override;

	enum class UniformType {
		FLOAT_MAT4 = GL_FLOAT_MAT4,
		FLOAT_VEC2 = GL_FLOAT_VEC2,
		FLOAT_VEC3 = GL_FLOAT_VEC3,
		SAMPLER_2D = GL_SAMPLER_2D,
		NOTFOUND
	};
	
	void makeActive() const;

	bool setUniform_m4(const std::string& name, const glm::mat4&) const;
	bool setUniform_v2(const std::string& name, const glm::vec2&) const;
	bool setUniform_v3(const std::string& name, const glm::vec3&) const;
	bool setUniform_i(const std::string& name, int) const;
	bool setUniform_f(const std::string& name, float) const;

	UniformType getUniformType(const std::string& name) const;
	int getAttribLocation(const std::string& name) const;

	static void invalidate()
	{
		s_activeProgram = std::numeric_limits<GLuint>::max();
	}

private:
	
	struct Uniform {
		GLint size;
		UniformType type;
		GLuint location;
	};

	struct Attribute {
		GLint size;
		UniformType type;
		GLuint location;
	};

	// fields

	GLuint m_program;

	std::map<std::string, Uniform> m_uniforms{};
	std::map<std::string, Attribute> m_attributes{};

	// static members
	
	// Only valid if glUseProgram is never called outside this class's method
	static GLuint s_activeProgram;

	// -1 if not found
	int getUniformLocation(const std::string& name) const;

};

}
