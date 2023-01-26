#version 450

layout(binding = 0) uniform UBO {
	mat4 proj;
} ubo;

layout( push_constant ) uniform Constants {
	mat4 model;
	mat4 view;
} constants;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNorm;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec3 fragNorm;

void main() {
	gl_Position = ubo.proj * constants.view * constants.model * vec4(inPosition, 1.0);
	fragNorm = mat3(transpose(inverse(constants.model))) * inNorm;
}
