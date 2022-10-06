#include "components/mesh_renderer.hpp"

#include "object.hpp"

#include "resource_manager.hpp"

#include <iostream>

namespace engine::components {

Renderer::Renderer(Object* parent) : Component(parent, TypeEnum::RENDERER)
{
	m_shader = this->parent.res.get<resources::Shader>("shaders/basic.glsl");
	m_texture = this->parent.res.get<resources::Texture>("textures/missing.png");
}

Renderer::~Renderer()
{

}

void Renderer::render(glm::mat4 transform)
{

	m_shader->setUniform_f("ambientStrength", 0.4f);
	m_shader->setUniform_v3("ambientColor", { 1.0f, 1.0f, 1.0f });

	m_shader->setUniform_v3("lightColor", { 1.0f, 1.0f, 1.0f });

	m_shader->setUniform_m4("modelMat", transform );

	m_texture->bindTexture();
	m_shader->setUniform_v3("baseColor", m_color );
	m_shader->setUniform_v3("emission", m_emission );

	if (m_mesh)
		m_mesh->drawMesh(*m_shader);
}

void Renderer::setMesh(const std::string& name)
{
	m_mesh = parent.res.get<resources::Mesh>(name);
}

void Renderer::setTexture(const std::string& name)
{
	m_texture = parent.res.get<resources::Texture>(name);
}

}
