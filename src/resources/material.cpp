#include "resources/material.hpp"

#include "resources/shader.hpp"

namespace engine::resources {

	Material::Material(std::shared_ptr<Shader> shader)
		: m_shader(shader)
	{

	}

	Material::~Material()
	{

	}

}
