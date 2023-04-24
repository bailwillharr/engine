#pragma once

#include "gfx_device.hpp"

#include <string>

namespace engine {
	struct RenderData;
}

namespace engine::resources {

class Texture {

public:
	enum class Filtering {
		OFF,
		BILINEAR,
		TRILINEAR,
		ANISOTROPIC,
	};

	Texture(RenderData* renderData, const std::string& path, Filtering filtering);
	~Texture();
	Texture(const Texture&) = delete;
	Texture& operator=(const Texture&) = delete;

	const gfx::Image* getImage() { return m_image; }
	const gfx::DescriptorSet* getDescriptorSet() { return m_descriptorSet; }

private:
	GFXDevice* m_gfxDevice;
	const gfx::Image* m_image;
	const gfx::DescriptorSet* m_descriptorSet;
};

}
