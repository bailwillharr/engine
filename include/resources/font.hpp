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

	struct CharData {
		glm::vec2 atlas_top_left{};
		glm::vec2 atlas_bottom_right{};
		glm::vec2 offset{};
		float xAdvance{};
	};

	const gfx::Texture* getAtlasTexture();

	CharData getCharData(uint32_t charCode);

private:
	const gfx::Texture* m_atlas;
	std::map<uint32_t, CharData> m_charData;

};

}
