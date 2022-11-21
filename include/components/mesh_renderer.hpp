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
	void render(glm::mat4 model, glm::mat4 view);

	void setMesh(const std::string& name);
	void setTexture(const std::string& name, bool invertV = false);

	std::shared_ptr<resources::Mesh> m_mesh = nullptr;
	std::shared_ptr<resources::Texture> m_texture;

private:

	std::shared_ptr<resources::Shader> m_shader;
	
};

}
