#version 450

layout( push_constant ) uniform Constants {
	mat4 model;
	vec2 offset;
	vec2 size;
} constants;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNorm;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec2 fragUV;

void main() {
	gl_Position = constants.model * vec4(inPosition, 1.0);
	fragUV = inUV;
}