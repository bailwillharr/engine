#pragma once

#include "gfx_device.hpp"

#include <string>

namespace engine::resources {

class Texture {

public:
	enum class Filtering {
		OFF,
		BILINEAR,
		TRILINEAR,
		ANISOTROPIC,
	};

	Texture(GFXDevice* gfxDevice, const std::string& path, Filtering filtering, bool useMipmaps, bool useLinearMagFilter);
	~Texture();
	Texture(const Texture&) = delete;
	Texture& operator=(const Texture&) = delete;

	gfx::Texture* getHandle();

private:
	GFXDevice* m_gfxDevice;
	gfx::Texture* m_gpuTexture;
};

}
