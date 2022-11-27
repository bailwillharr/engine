#pragma once

#include "engine_api.h"

#include "resource.hpp"

#include "gfx.hpp"

#include <glm/vec2.hpp>

#include <map>

namespace engine::resources {

class ENGINE_API Font : public Resource {

public:

	Font(const std::filesystem::path& resPath);
	~Font() override;

	const gfx::Texture* getAtlasTexture();

private:
	const gfx::Texture* m_atlas;

};

}
