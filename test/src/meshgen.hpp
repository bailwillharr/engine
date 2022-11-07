#pragma once

#include "resources/mesh.hpp"

// generates a UV sphere
std::unique_ptr<engine::resources::Mesh> genSphereMesh(float r, int detail, bool windInside = false);
std::unique_ptr<engine::resources::Mesh> genCuboidMesh(float x, float y, float z);
