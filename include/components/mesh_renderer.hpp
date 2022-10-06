#pragma once

#include "engine_api.h"

#include "component.hpp"

#include "resources/shader.hpp"
#include "resources/mesh.hpp"
#include "resources/texture.hpp"

#include <vector>
#include <string>
#include <memory>

namespace engine::components {

class ENGINE_API Renderer : public Component {

public:
	Renderer(Object*);
	~Renderer() override;

	// called every frame, do not call manually
	void render(glm::mat4 transform);

	void setMesh(const std::string& name);
	void setTexture(const std::string& name);

	std::shared_ptr<resources::Mesh> m_mesh = nullptr;
	std::shared_ptr<resources::Texture> m_texture;

	glm::vec3 m_color = { 1.0f, 1.0f, 1.0f };
	glm::vec3 m_emission = { 0.0f, 0.0f, 0.0f };

private:

	std::shared_ptr<resources::Shader> m_shader;
	
};

}
