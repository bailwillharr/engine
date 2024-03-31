#pragma once

#include <memory>

#include "resources/mesh.h"

std::unique_ptr<engine::Mesh> GenSphereMesh(engine::GFXDevice* gfx, float r, int detail, bool wind_inside = false, bool flip_normals = false);

std::unique_ptr<engine::Mesh> GenCuboidMesh(engine::GFXDevice* gfx, float x, float y, float z, float tiling = 1.0f, bool wind_inside = false);