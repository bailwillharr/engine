#pragma once

#include "engine_api.h"

#include "resource.hpp"

#include <glad/glad.h>

namespace engine::resources {

class ENGINE_API Texture : public Resource {

private:

	static GLuint s_binded_texture;

	GLuint m_texture;


public:
	Texture(const std::filesystem::path& resPath);
	~Texture() override;

	void bindTexture() const;

	static void invalidate();
};

}
