#include "meshgen.hpp"

#include <glm/gtc/constants.hpp>
#include <glm/ext.hpp>
#include <glm/trigonometric.hpp>

#include <iostream>

#include <thread>

std::unique_ptr<engine::resources::Mesh> genSphereMesh(float r, int detail, bool windInside)
{
	using namespace glm;

	std::vector<Vertex> vertices{};

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
			
			glm::vec3 vector1 = (vertices.end() - 1)->pos - (vertices.end() - 2)->pos;
			glm::vec3 vector2 = (vertices.end() - 2)->pos - (vertices.end() - 3)->pos;
			glm::vec3 norm = glm::normalize(glm::cross(vector1, vector2));


			// TODO: FIX NORMALS
			if (!windInside)
				norm = -norm;

			for (auto it = vertices.end() - 6; it != vertices.end(); it++) {
				it->norm = norm;
			}

		}
	}

	return std::make_unique<engine::resources::Mesh>(vertices);
}

std::unique_ptr<engine::resources::Mesh> genCuboidMesh(float x, float y, float z)
{

	// x goes ->
	// y goes ^
	// z goes into the screen

	using glm::vec3;

	std::vector<Vertex> v{};

	// 0 top_left_front
	v.push_back({{ 0.0f,	y,		0.0f	}, {}, {}});
	// 1 bottom_left_front
	v.push_back({{ 0.0f,	0.0f,	0.0f	}, {}, {}});
	// 2 top_right_front
	v.push_back({{ x,		y,		0.0f	}, {}, {}});
	// 3 bottom_right_front
	v.push_back({{ x,		0.0f,	0.0f	}, {}, {}});

	// 4 top_left_back
	v.push_back({{ 0.0f,	y,		z		}, {}, {}});
	// 5 bottom_left_back
	v.push_back({{ 0.0f,	0.0f,	z		}, {}, {}});
	// 6 top_right_back
	v.push_back({{ x,		y,		z		}, {}, {}});
	// 7 bottom_right_back
	v.push_back({{ x,		0.0f,	z		}, {}, {}});

	// front quad
	std::vector<unsigned int> indices{
		// front
		0, 1, 3, 2, 0, 3,
		// back
		4, 5, 7, 6, 4, 7,
		// bottom
		5, 1, 3, 7, 5, 3,
		// top
		4, 0, 2, 6, 4, 2,
		// left
		4, 5, 1, 0, 4, 1,
		// right
		2, 3, 7, 6, 2, 7
	};

	return std::make_unique<engine::resources::Mesh>(v, indices);

}
