#pragma once

#include <memory>

#include "gfx_device.h"
#include "resource_mesh.h"

std::unique_ptr<engine::Mesh> genTerrainChunk(engine::GFXDevice* gfx, float x_offset, float y_offset, float uv_scale, unsigned int seed);