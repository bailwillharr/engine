#include "components/mesh_renderer.hpp"

#include "object.hpp"

#include "resource_manager.hpp"

#include "gfx_device.hpp"

#include "log.hpp"

#include <iostream>

namespace engine::components {

Renderer::Renderer(Object* parent) : Component(parent, TypeEnum::RENDERER)
{
	m_shader = this->parent.res.get<resources::Shader>("shader.glsl");
//	m_texture = this->parent.res.get<resources::Texture>("textures/missing.png");
}

Renderer::~Renderer()
{

}

void Renderer::render(glm::mat4 transform)
{
	gfxdev->draw(m_shader->getPipeline(), m_mesh->vb, m_mesh->ib, m_mesh->m_vertices.size(), &transform);
}

void Renderer::setMesh(const std::string& name)
{
	m_mesh = parent.res.get<resources::Mesh>(name);
}

void Renderer::setTexture(const std::string& name)
{
//	m_texture = parent.res.get<resources::Texture>(name);
}

}
