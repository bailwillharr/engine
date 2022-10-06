#include "components/text_ui_renderer.hpp"

#include "object.hpp"
#include "resource_manager.hpp"
#include "resources/texture.hpp"

namespace engine::components {

UI::UI(Object* parent) : Component(parent, TypeEnum::UI)
{
	const std::string FONTFILE{ "fonts/LiberationMono-Regular.ttf" };
	m_font = parent->res.get<resources::Font>(FONTFILE);
	m_shader = parent->res.get<resources::Shader>("shaders/font.glsl");

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

UI::~UI()
{
}

void UI::render(glm::mat4 transform)
{

	glActiveTexture(GL_TEXTURE0);
	m_shader->setUniform_m4("modelMat", transform);

	m_shader->setUniform_v3("textColor", m_color);
	m_shader->setUniform_i("textScaling", (int)m_scaled);

	std::vector<resources::Font::Character> glyphs;
	for (char c : m_text) {
		glyphs.push_back(m_font->getChar(c));
	}

	constexpr float scale = 0.002f;
	float x = 0.0f;

	if (m_alignment == Alignment::RIGHT) {
		
		// first find the length of the text
		float len = 0.0f;
		for (const auto& glyph : glyphs) {
			len += (glyph.advance >> 6) * scale;
		}

		x = -len;

	}

	for (const auto& glyph : glyphs) {

		float xpos = x + glyph.bearing.x * scale;
		float ypos = (glyph.bearing.y - glyph.size.y) * scale;

		float w = glyph.size.x * scale;
		float h = glyph.size.y * scale;

		resources::Mesh mesh({
			{{xpos,		ypos + h,	0.0f}, {}, {0.0f, 0.0f}},
			{{xpos,		ypos	,	0.0f}, {}, {0.0f, 1.0f}},
			{{xpos + w,	ypos,		0.0f}, {}, {1.0f, 1.0f}},
			{{xpos,		ypos + h,	0.0f}, {}, {0.0f, 0.0f}},
			{{xpos + w,	ypos,		0.0f}, {}, {1.0f, 1.0f}},
			{{xpos + w,	ypos + h,	0.0f}, {}, {1.0f, 0.0f}},
		});

		glBindTexture(GL_TEXTURE_2D, glyph.textureID);

		mesh.drawMesh(*m_shader);

		x += (glyph.advance >> 6) * scale;

	}

	resources::Texture::invalidate();

}

}
