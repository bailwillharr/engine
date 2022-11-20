#include "components/mesh_renderer.hpp"

#include "resources/shader.hpp"

#include "object.hpp"

#include "resource_manager.hpp"

#include "gfx_device.hpp"

#include "log.hpp"

#include <iostream>

namespace engine::components {

Renderer::Renderer(Object* parent) : Component(parent, TypeEnum::RENDERER)
{
	m_shader = this->parent.res.get<resources::Shader>("shader.glsl");
	m_texture = this->parent.res.get<resources::Texture>("textures/missing.png");
}

Renderer::~Renderer()
{

}

void Renderer::render(glm::mat4 transform, glm::mat4 view)
{
	resources::Shader::UniformBuffer uniformData{};
	uniformData.color = glm::vec4{ m_color.r, m_color.g, m_color.b, 1.0 };
	gfxdev->updateUniformBuffer(m_shader->getPipeline(), &uniformData.color, sizeof(uniformData.color), offsetof(resources::Shader::UniformBuffer, color));

	glm::mat4 pushConsts[] = { transform, view };
	gfxdev->draw(m_shader->getPipeline(), m_mesh->vb, m_mesh->ib, m_mesh->m_indices.size(), pushConsts, sizeof(glm::mat4) * 2, m_texture->getHandle());
}

void Renderer::setMesh(const std::string& name)
{
	m_mesh = parent.res.get<resources::Mesh>(name);
}

void Renderer::setTexture(const std::string& name, bool invertV)
{
	if (invertV) {
		// append a special character to file name
		m_texture = parent.res.get<resources::Texture>(name + "_");
	}
	else {
		m_texture = parent.res.get<resources::Texture>(name);
	}
}

}
