#pragma once

#include "engine_api.h"

#include "resource.hpp"

#include "gfx.hpp"

#include <glm/vec2.hpp>

#include <map>

namespace engine::resources {

class ENGINE_API Font : public Resource {

public:

	struct Character {
		gfx::Texture* texture;
		glm::ivec2 size;
		glm::ivec2 bearing; // offset from baseline to top-left of glyph
		long advance; // offset to the next glyph
	};

	Font(const std::filesystem::path& resPath);
	~Font() override;
	Character getChar(char c);

	private:
	std::map<char, Character> m_characters;

};

}
