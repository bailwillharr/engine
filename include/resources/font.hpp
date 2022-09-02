#pragma once

#include "resource.hpp"

#include <glad/glad.h>

#include <glm/vec2.hpp>

#include <map>

namespace resources {

class Font : public Resource {

public:

	struct Character {
		unsigned int textureID; // openGL texture handle
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
