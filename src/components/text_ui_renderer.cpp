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
		{ { 1.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f } },
		{ { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f } },
		{ { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f } },
		{ { 1.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f } },
		{ { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f } },
		{ { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 1.0f } }
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

	int advance = 0;

	for (char c : m_text) {
		auto charData = m_font->getCharData(c);
		pushConsts.m = glm::mat4{1.0f};
		pushConsts.atlas_top_left = charData.atlas_top_left;
		pushConsts.atlas_bottom_right = charData.atlas_bottom_right;
		pushConsts.offset = charData.offset;
		gfxdev->draw(
				m_shader->getPipeline(), m_atlasMesh->vb, m_atlasMesh->ib, m_atlasMesh->m_indices.size(),
				&pushConsts, sizeof(pushConsts), m_font->getAtlasTexture());
		advance += charData.xAdvance;
	}

}

}
