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

	m_atlasMesh = std::make_unique<resources::Mesh>(std::vector<Vertex>{
		{ { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f } },
		{ { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f } },
		{ { 1.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 1.0f } },
		
		{ { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f } },
		{ { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f } },
		{ { 1.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 1.0f } },
	});
}

UI::~UI()
{

}

void UI::render(glm::mat4 transform, glm::mat4 view)
{

	struct {
		glm::mat4 m;
		glm::vec2 atlas_top_left;
		glm::vec2 atlas_bottom_right;
		glm::vec2 offset;
		glm::vec2 size;
	} pushConsts{};

	float advance = 0.0f;
	for (char c : m_text) {
		auto charData = m_font->getCharData(c);
		charData.char_top_left.y *= -1;
		pushConsts.m = transform;
		pushConsts.atlas_top_left = charData.atlas_top_left;
		pushConsts.atlas_bottom_right = charData.atlas_bottom_right;
		pushConsts.size = (charData.char_bottom_right - charData.char_top_left);
		pushConsts.offset = glm::vec2{ charData.char_top_left.x + advance, charData.char_top_left.y };
		gfxdev->draw(
				m_shader->getPipeline(), m_atlasMesh->vb, m_atlasMesh->ib, m_atlasMesh->m_indices.size(),
				&pushConsts, sizeof(pushConsts), m_font->getAtlasTexture());
		advance += charData.xAdvance;
	}

}

}
