#pragma once

#include <cstdint>

#include <vector>

namespace engine {

struct Vertex; // forward-dec

/*
 * Generate tangents for a given list of vertices.
 * The provided vertices must be in proper order.
 * Parameters:
 *     vertices (in/out) - vertices to modify with generated tangents (size can change)
 * Returns:
 *     index list for the provided vertices
 */
std::vector<uint32_t> genTangents(std::vector<engine::Vertex>& vertices);

} // namespace engine