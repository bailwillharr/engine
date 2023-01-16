#include "resources/material.hpp"

#include "resources/shader.hpp"

namespace engine::resources {

	Material::Material(std::shared_ptr<Shader> shader)
		: m_shader(shader)
	{

	}

	Material::Material(const Material& original)
		:	m_texture(original.m_texture), m_shader(original.m_shader)
	{

	}

}
