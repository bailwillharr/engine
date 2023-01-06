#include "meshgen.hpp"

#include <glm/gtc/constants.hpp>
#include <glm/ext.hpp>
#include <glm/trigonometric.hpp>

#include <stdexcept>

std::unique_ptr<engine::resources::Mesh> genSphereMesh(engine::GFXDevice* gfx, float r, int detail, bool windInside, bool flipNormals)
{
	using namespace glm;

	std::vector<engine::Vertex> vertices{};

	float angleStep = two_pi<float>() / (float)detail;

	for (int i = 0; i < detail; i++) {
		// theta goes north-to-south
		float theta = i * angleStep;
		float theta2 = theta + angleStep;
		for (int j = 0; j < detail/2; j++) {
			// phi goes west-to-east
			float phi = j * angleStep;
			float phi2 = phi + angleStep;

			vec3 top_left{		r * sin(phi)  * cos(theta),
								r * cos(phi),
								r * sin(phi)  * sin(theta)	};
			vec3 bottom_left{	r * sin(phi)  * cos(theta2),
								r * cos(phi),
								r * sin(phi)  * sin(theta2)	};
			vec3 top_right{		r * sin(phi2) * cos(theta),
								r * cos(phi2),
								r * sin(phi2) * sin(theta)	};
			vec3 bottom_right{	r * sin(phi2) * cos(theta2),
								r * cos(phi2),
								r * sin(phi2) * sin(theta2)	};

			if (windInside == false) {
				// tris are visible from outside the sphere

				// triangle 1
				vertices.push_back({ top_left, {}, {0.0f, 0.0f} });
				vertices.push_back({ bottom_left, {}, {0.0f, 1.0f} });
				vertices.push_back({ bottom_right, {}, {1.0f, 1.0f} });
				// triangle 2
				vertices.push_back({ top_right, {}, {1.0f, 0.0f} });
				vertices.push_back({ top_left, {}, {0.0f, 0.0f} });
				vertices.push_back({ bottom_right, {}, {1.0f, 1.0f} });

			}
			else {
				// tris are visible from inside the sphere

				// triangle 1
				vertices.push_back({ bottom_right, {}, {1.0f, 1.0f} });
				vertices.push_back({ bottom_left, {}, {0.0f, 1.0f} });
				vertices.push_back({ top_left, {}, {0.0f, 0.0f} });
				
				// triangle 2
				vertices.push_back({ bottom_right, {}, {1.0f, 1.0f} });
				vertices.push_back({ top_left, {}, {0.0f, 0.0f} });
				vertices.push_back({ top_right, {}, {1.0f, 0.0f} });

			}
			
			vec3 vector1 = (vertices.end() - 1)->pos - (vertices.end() - 2)->pos;
			vec3 vector2 = (vertices.end() - 2)->pos - (vertices.end() - 3)->pos;
			vec3 norm = normalize(cross(vector1, vector2));


			// TODO: FIX NORMALS
			if (!windInside)
				norm = -norm;

			if (flipNormals)
				norm = -norm;

			for (auto it = vertices.end() - 6; it != vertices.end(); it++) {
				it->norm = norm;
			}

		}
	}

	return std::make_unique<engine::resources::Mesh>(gfx, vertices);
}

std::unique_ptr<engine::resources::Mesh> genCuboidMesh(engine::GFXDevice* gfx, float x, float y, float z)
{

	// x goes ->
	// y goes ^
	// z goes into the screen

	using namespace glm;

	std::vector<engine::Vertex> v{};

	// front
	v.push_back({{x,	0.0f,	0.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}});
	v.push_back({{0.0f,	0.0f,	0.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}});
	v.push_back({{0.0f,	y,		0.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}});
	v.push_back({{x,	0.0f,	0.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}});
	v.push_back({{0.0f,	y,		0.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}});
	v.push_back({{x,	y,		0.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}});

	// back
	v.push_back({{0.0f,	0.0f,	z}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}});
	v.push_back({{x,	0.0f,	z}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}});
	v.push_back({{0.0f,	y,		z}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}});
	v.push_back({{x,	0.0f,	z}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}});
	v.push_back({{x,	y,		z}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}});
	v.push_back({{0.0f,	y,		z}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}});

	// left
	v.push_back({{0.0f,	0.0f,	0.0f},	{-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}});
	v.push_back({{0.0f,	0.0f,	x},		{-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}});
	v.push_back({{0.0f,	y,		0.0f},	{-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}});
	v.push_back({{0.0f,	0.0f,	x},		{-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}});
	v.push_back({{0.0f,	y,		x},		{-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}});
	v.push_back({{0.0f,	y,		0.0f},	{-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}});

	// right
	v.push_back({{x,	y,		0.0f},	{1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}});
	v.push_back({{x,	0.0f,	x},		{1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}});
	v.push_back({{x,	0.0f,	0.0f},	{1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}});
	v.push_back({{x,	y,		0.0f},	{1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}});
	v.push_back({{x,	y,		x},		{1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}});
	v.push_back({{x,	0.0f,	x},		{1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}});

	// bottom
	v.push_back({{0.0f,	0.0f,	z},		{0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}});
	v.push_back({{0.0f,	0.0f,	0.0f},	{0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}});
	v.push_back({{x,	0.0f,	0.0f},	{0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}});
	v.push_back({{x,	0.0f,	z},		{0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}});
	v.push_back({{0.0f,	0.0f,	z},		{0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}});
	v.push_back({{x,	0.0f,	0.0f},	{0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}});

	// top
	v.push_back({{x,	y,	0.0f},	{0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}});
	v.push_back({{0.0f,	y,	0.0f},	{0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}});
	v.push_back({{0.0f,	y,	z},		{0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}});
	v.push_back({{x,	y,	0.0f},	{0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}});
	v.push_back({{0.0f,	y,	z},		{0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}});
	v.push_back({{x,	y,	z},		{0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}});

	return std::make_unique<engine::resources::Mesh>(gfx, v);

}
