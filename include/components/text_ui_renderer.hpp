#pragma once

#include "export.h"

#include "component.hpp"

#include "resources/font.hpp"
#include "resources/mesh.hpp"

#include <glm/mat4x4.hpp>

namespace components {

class ENGINE_API UI : public Component {

public:
	UI(Object*);
	~UI() override;

	// called every frame, do not call manually
	void render(glm::mat4 transform);

	std::string m_text = "hello world";
	glm::vec3 m_color = {1.0f, 1.0f, 1.0f};

	enum class Alignment {
		NONE = 0,
		LEFT = 1,
		RIGHT = 2
	};
	Alignment m_alignment = Alignment::LEFT;
	bool m_scaled = true;

private:
	std::shared_ptr<resources::Font> m_font;
	std::shared_ptr<resources::Shader> m_shader;

};

}
