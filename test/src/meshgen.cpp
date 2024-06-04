#include "meshgen.hpp"

#include <stdexcept>

#include <glm/gtc/constants.hpp>
#include <glm/ext.hpp>
#include <glm/trigonometric.hpp>

#include "resource_mesh.h"
#include "gen_tangents.h"

std::unique_ptr<engine::Mesh> GenSphereMesh(engine::GFXDevice* gfx, float r, int detail, bool wind_inside, bool flip_normals)
{
    using namespace glm;

    std::vector<engine::Vertex> vertices{};

    const float angle_step = two_pi<float>() / (float)detail;

    for (int i = 0; i < detail; i++) {
        // theta goes north-to-south
        float theta = i * angle_step;
        float theta2 = theta + angle_step;
        for (int j = 0; j < detail / 2; j++) {
            // phi goes west-to-east
            float phi = j * angle_step;
            float phi2 = phi + angle_step;

            vec3 top_left{r * sin(phi) * cos(theta), r * cos(phi), r * sin(phi) * sin(theta)};
            vec3 bottom_left{r * sin(phi) * cos(theta2), r * cos(phi), r * sin(phi) * sin(theta2)};
            vec3 top_right{r * sin(phi2) * cos(theta), r * cos(phi2), r * sin(phi2) * sin(theta)};
            vec3 bottom_right{r * sin(phi2) * cos(theta2), r * cos(phi2), r * sin(phi2) * sin(theta2)};

            if (wind_inside == false) {
                // tris are visible from outside the sphere

                // triangle 1
                vertices.push_back({top_left, {}, {}, {0.0f, 0.0f}});
                vertices.push_back({bottom_left, {}, {}, {0.0f, 1.0f}});
                vertices.push_back({bottom_right, {}, {}, {1.0f, 1.0f}});
                // triangle 2
                vertices.push_back({top_right, {}, {}, {1.0f, 0.0f}});
                vertices.push_back({top_left, {}, {}, {0.0f, 0.0f}});
                vertices.push_back({bottom_right, {}, {}, {1.0f, 1.0f}});
            }
            else {
                // tris are visible from inside the sphere

                // triangle 1
                vertices.push_back({bottom_right, {}, {}, {1.0f, 1.0f}});
                vertices.push_back({bottom_left, {}, {}, {0.0f, 1.0f}});
                vertices.push_back({top_left, {}, {}, {0.0f, 0.0f}});

                // triangle 2
                vertices.push_back({bottom_right, {}, {}, {1.0f, 1.0f}});
                vertices.push_back({top_left, {}, {}, {0.0f, 0.0f}});
                vertices.push_back({top_right, {}, {}, {1.0f, 0.0f}});
            }

            vec3 vector1 = (vertices.end() - 1)->pos - (vertices.end() - 2)->pos;
            vec3 vector2 = (vertices.end() - 2)->pos - (vertices.end() - 3)->pos;
            vec3 norm = normalize(cross(vector2, vector1));

            // NORMALS HAVE BEEN FIXED

            if (flip_normals) norm = -norm;

            if (j == (detail / 2) - 1) norm = -norm;

            for (auto it = vertices.end() - 6; it != vertices.end(); ++it) {
                it->norm = norm;
            }
        }
    }

    std::vector<uint32_t> indices = engine::genTangents(vertices);

    return std::make_unique<engine::Mesh>(gfx, vertices, indices);
}

std::unique_ptr<engine::Mesh> GenCuboidMesh(engine::GFXDevice* gfx, float x, float y, float z, float tiling, bool wind_inside)
{
    // x goes ->
    // y goes ^
    // z goes into the screen

    using namespace glm;

    std::vector<engine::Vertex> vertices{};

    // front
    vertices.push_back({{0.0f, 0.0f, z}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}});
    vertices.push_back({{0.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, tiling}});
    vertices.push_back({{x, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {tiling, tiling}});
    vertices.push_back({{x, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {tiling, tiling}});
    vertices.push_back({{x, 0.0f, z}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {tiling, 0.0f}});
    vertices.push_back({{0.0f, 0.0f, z}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}});
    // back
    vertices.push_back({{x, y, z}, {0.0f, 1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}});
    vertices.push_back({{x, y, 0.0f}, {0.0f, 1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, tiling}});
    vertices.push_back({{0.0f, y, 0.0f}, {0.0f, 1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}, {tiling, tiling}});
    vertices.push_back({{0.0f, y, 0.0f}, {0.0f, 1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}, {tiling, tiling}});
    vertices.push_back({{0.0f, y, z}, {0.0f, 1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}, {tiling, 0.0f}});
    vertices.push_back({{x, y, z}, {0.0f, 1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}});
    // left
    vertices.push_back({{0.0f, y, z}, {-1.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f, 1.0f}, {0.0f, 0.0f}});
    vertices.push_back({{0.0f, y, 0.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f, 1.0f}, {0.0f, tiling}});
    vertices.push_back({{0.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f, 1.0f}, {tiling, tiling}});
    vertices.push_back({{0.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f, 1.0f}, {tiling, tiling}});
    vertices.push_back({{0.0f, 0.0f, z}, {-1.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f, 1.0f}, {tiling, 0.0f}});
    vertices.push_back({{0.0f, y, z}, {-1.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f, 1.0f}, {0.0f, 0.0f}});
    // right
    vertices.push_back({{x, 0.0f, z}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 0.0f}});
    vertices.push_back({{x, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {0.0f, tiling}});
    vertices.push_back({{x, y, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {tiling, tiling}});
    vertices.push_back({{x, y, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {tiling, tiling}});
    vertices.push_back({{x, y, z}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {tiling, 0.0f}});
    vertices.push_back({{x, 0.0f, z}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 0.0f}});
    // top
    vertices.push_back({{0.0f, y, z}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}});
    vertices.push_back({{0.0f, 0.0f, z}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, tiling}});
    vertices.push_back({{x, 0.0f, z}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {tiling, tiling}});
    vertices.push_back({{x, 0.0f, z}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {tiling, tiling}});
    vertices.push_back({{x, y, z}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {tiling, 0.0f}});
    vertices.push_back({{0.0f, y, z}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}});
    // bottom
    vertices.push_back({{x, y, 0.0f}, {0.0f, 0.0f, -1.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}});
    vertices.push_back({{x, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, tiling}});
    vertices.push_back({{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}, {tiling, tiling}});
    vertices.push_back({{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}, {tiling, tiling}});
    vertices.push_back({{0.0f, y, 0.0f}, {0.0f, 0.0f, -1.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}, {tiling, 0.0f}});
    vertices.push_back({{x, y, 0.0f}, {0.0f, 0.0f, -1.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}});

    if (wind_inside) {
        for (size_t i = 0; i < vertices.size(); i += 3) {
            std::swap(vertices[i], vertices[i + 2]);
        }
    }

    std::vector<uint32_t> indices = engine::genTangents(vertices);

    return std::make_unique<engine::Mesh>(gfx, vertices, indices);
}
