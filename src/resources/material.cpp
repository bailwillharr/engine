#include "resources/material.h"

#include "resources/shader.h"

namespace engine::resources {

	Material::Material(std::shared_ptr<Shader> shader)
		: shader_(shader)
	{

	}

	Material::Material(const Material& original)
		:	texture_(original.texture_), shader_(original.shader_)
	{

	}

}
