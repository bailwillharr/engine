#pragma once

#include "resources/mesh.hpp"

// generates a UV sphere
std::unique_ptr<engine::resources::Mesh> genSphereMesh(engine::GFXDevice* gfx, float r, int detail, bool windInside = false, bool flipNormals = false);
std::unique_ptr<engine::resources::Mesh> genCuboidMesh(engine::GFXDevice* gfx, float x, float y, float z);
