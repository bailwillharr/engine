#include "terrain.h"

#include <memory>

#include <glm/geometric.hpp>
#include <PerlinNoise.hpp>

#include "gen_tangents.h"
#include "gfx_device.h"
#include "log.h"
#include "resource_mesh.h"

static constexpr int RES = 100; // number of vertices along an axis

static glm::vec3 getNormal(const std::vector<float>& heightmap, int x, int y)
{

    // there are eight surrounding normals per vertex
    // need to use x+1 and y+1 for sampling heightmap at a point

    constexpr float size = 1.0f / (float)(RES - 1);

    glm::vec3 sum{};
    for (int y00 = y - 1; y00 <= y; ++y00) {
        for (int x00 = x - 1; x00 <= x; ++x00) {
            float z00 = heightmap[(y00 + 1) * (RES + 2) + x00 + 1];
            float z01 = heightmap[(y00 + 2) * (RES + 2) + x00 + 1];
            float z10 = heightmap[(y00 + 1) * (RES + 2) + x00 + 2];
            float z11 = heightmap[(y00 + 2) * (RES + 2) + x00 + 2];
            glm::vec3 v00{x00 * size, y00 * size, z00};
            glm::vec3 v01{x00 * size, (y00 + 1) * size, z01};
            glm::vec3 v10{(x00 + 1) * size, y00 * size, z10};
            glm::vec3 v11{(x00 + 1) * size, (y00 + 1) * size, z11};
            sum += glm::normalize(glm::cross(v10 - v00, v01 - v00));
            sum += glm::normalize(glm::cross(v01 - v11, v10 - v11));
        }
    }

    return sum / 8.0f;
}

std::unique_ptr<engine::Mesh> genTerrainChunk(engine::GFXDevice* gfx, float x_offset, float y_offset, float uv_scale, unsigned int seed)
{
    static_assert(sizeof(siv::PerlinNoise::seed_type) >= sizeof(unsigned int));

    const siv::PerlinNoise perlin(static_cast<siv::PerlinNoise::seed_type>(seed));

    std::vector<float> heightmap((RES + 2) * (RES + 2)); // need two more points per axis for normals
    std::vector<glm::vec3> normalmap(RES * RES);

    for (int y = 0; y < RES + 2; ++y) {
        for (int x = 0; x < RES + 2; ++x) {
            float noise = (float)perlin.octave2D_01(((x - 1) / ((float)RES - 1)) + x_offset, ((y - 1) / ((float)RES - 1)) + y_offset, 4);
            noise *= 1.5f;
            noise = fmaxf(1.0f, noise);
            noise -= 1.0f;
            //noise *= 5.0f;
            noise = glm::clamp(noise, 0.0f, 0.5f);
            noise *= 2.0f;
            heightmap[y * (RES + 2) + x] = noise;
        }
    }

    for (int y = 0; y < RES; ++y) {
        for (int x = 0; x < RES; ++x) {
            normalmap[y * RES + x] = getNormal(heightmap, x, y);
            //normalmap[y * RES + x] = glm::vec3{0.0f, 0.0f, 1.0f};
        }
    }

    std::vector<engine::Vertex> vertices{};
    vertices.reserve(RES * RES * 6); // 1 triangle per height point for now
    const float size = 1.0f / (float)(RES - 1);

    for (int y = 0; y < RES - 1; ++y) {
        for (int x = 0; x < RES - 1; ++x) {
            const float z00 = heightmap[y * (RES + 2) + x];
            const float z01 = heightmap[(y + 1) * (RES + 2) + x];
            const float z10 = heightmap[y * (RES + 2) + x + 1];
            const float z11 = heightmap[(y + 1) * (RES + 2) + x + 1];

            vertices.push_back(engine::Vertex{{x, y, z00}, {0.0f, 0.0f, 1.0f}, {/* tangents gen'd after */}, {0.0f, 0.0f}});
            vertices.back().pos.x *= size;
            vertices.back().pos.y *= size;
            vertices.back().uv.x = vertices.back().pos.x * uv_scale;
            vertices.back().uv.y = vertices.back().pos.y * uv_scale;
            vertices.push_back(engine::Vertex{{x + 1.0f, y, z10}, {0.0f, 0.0f, 1.0f}, {/* tangents gen'd after */}, {0.0f, 0.0f}});
            vertices.back().pos.x *= size;
            vertices.back().pos.y *= size;
            vertices.back().uv.x = vertices.back().pos.x * uv_scale;
            vertices.back().uv.y = vertices.back().pos.y * uv_scale;
            vertices.push_back(engine::Vertex{{x, y + 1.0f, z01}, {0.0f, 0.0f, 1.0f}, {/* tangents gen'd after */}, {0.0f, 0.0f}});
            vertices.back().pos.x *= size;
            vertices.back().pos.y *= size;
            vertices.back().uv.x = vertices.back().pos.x * uv_scale;
            vertices.back().uv.y = vertices.back().pos.y * uv_scale;

            // compute flat normals

            (vertices.end() - 3)->norm = normalmap[y * RES + x];
            (vertices.end() - 2)->norm = normalmap[y * RES + x + 1];
            (vertices.end() - 1)->norm = normalmap[(y + 1) * RES + x];

            vertices.push_back(engine::Vertex{{x + 1.0f, y + 1.0f, z11}, {0.0f, 0.0f, 1.0f}, {/* tangents gen'd after */}, {0.0f, 0.0f}});
            vertices.back().pos.x *= size;
            vertices.back().pos.y *= size;
            vertices.back().uv.x = vertices.back().pos.x * uv_scale;
            vertices.back().uv.y = vertices.back().pos.y * uv_scale;
            vertices.push_back(engine::Vertex{{x, y + 1.0f, z01}, {0.0f, 0.0f, 1.0f}, {/* tangents gen'd after */}, {0.0f, 0.0f}});
            vertices.back().pos.x *= size;
            vertices.back().pos.y *= size;
            vertices.back().uv.x = vertices.back().pos.x * uv_scale;
            vertices.back().uv.y = vertices.back().pos.y * uv_scale;
            vertices.push_back(engine::Vertex{{x + 1.0f, y, z10}, {0.0f, 0.0f, 1.0f}, {/* tangents gen'd after */}, {0.0f, 0.0f}});
            vertices.back().pos.x *= size;
            vertices.back().pos.y *= size;
            vertices.back().uv.x = vertices.back().pos.x * uv_scale;
            vertices.back().uv.y = vertices.back().pos.y * uv_scale;

            (vertices.end() - 3)->norm = normalmap[(y + 1) * RES + x + 1];
            (vertices.end() - 2)->norm = normalmap[(y + 1) * RES + x];
            (vertices.end() - 1)->norm = normalmap[y * RES + x + 1];
        }
    }

    std::vector<uint32_t> indices = engine::genTangents(vertices);

    return std::make_unique<engine::Mesh>(gfx, vertices, indices);
}
