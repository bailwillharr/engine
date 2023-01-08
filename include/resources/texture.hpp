#pragma once

#include "gfx_device.hpp"

#include <string>

namespace engine::resources {

class Texture {

public:
	Texture(GFXDevice* gfxDevice, const std::string& path);
	~Texture();
	Texture(const Texture&) = delete;
	Texture& operator=(const Texture&) = delete;

	gfx::Texture* getHandle();

private:
	GFXDevice* m_gfxDevice;
	gfx::Texture* m_gpuTexture;
};

}
