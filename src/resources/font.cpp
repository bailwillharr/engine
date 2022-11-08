#if 0
#include "resources/font.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H

namespace engine::resources {

Font::Font(const std::filesystem::path& resPath) : Resource(resPath, "font")
{

	FT_Library library;
	FT_Face face;
	int err;

	err = FT_Init_FreeType(&library);
	if (err) {
		throw std::runtime_error("Failed to initialise freetype library");
	}

	err = FT_New_Face(library, resPath.string().c_str(), 0, &face);
	if (err == FT_Err_Unknown_File_Format) {
		FT_Done_FreeType(library);
		throw std::runtime_error("Unknown file format for font '" + resPath.string() + "'");
	}
	else if (err != 0) {
		FT_Done_FreeType(library);
		throw std::runtime_error("Unable to open font '" + resPath.string() + "'");
	}

	err = FT_Set_Pixel_Sizes(face, 0, 64);
	if (err) {
		FT_Done_Face(face);
		FT_Done_FreeType(library);
		throw std::runtime_error("Attempt to set pixel size to one unavailable in the font");
	}

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	for (unsigned char c = 0; c < 128; c++) {

		err = FT_Load_Char(face, c, FT_LOAD_RENDER);
		if (err) {
			FT_Done_Face(face);
			FT_Done_FreeType(library);
			throw std::runtime_error("Unable to load char glyph");
		}

		// generate texture
		unsigned int texture;
		glGenTextures(1, &texture);
		glBindTexture(GL_TEXTURE_2D, texture);
		glTexImage2D(
				GL_TEXTURE_2D,
				0,
				GL_RED,
				face->glyph->bitmap.width,
				face->glyph->bitmap.rows,
				0,
				GL_RED,
				GL_UNSIGNED_BYTE,
				face->glyph->bitmap.buffer
		);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		Character character = {
			texture,
			glm::ivec2{face->glyph->bitmap.width, face->glyph->bitmap.rows},	// Size of Glyph
			glm::ivec2{face->glyph->bitmap_left, face->glyph->bitmap_top},		// Offset from baseline (bottom-left) to top-left of glyph
			face->glyph->advance.x
		};

		m_characters.insert(std::make_pair(c, character));

	}

	glPixelStorei(GL_UNPACK_ALIGNMENT, 4); // reset alignment settings

	FT_Done_Face(face);
	FT_Done_FreeType(library);

}

Font::~Font()
{
}

Font::Character Font::getChar(char c)
{
	return m_characters.at(c);
}

}
#endif