#pragma once

#include "engine_api.h"

#include "resource.hpp"

#include "gfx_device.hpp"

namespace engine::resources {

class ENGINE_API Texture : public Resource {

public:
	Texture(const std::filesystem::path& resPath);
	~Texture() override;

private:
	gfx::Texture* m_gpuTexture;
};

}