#version 450

layout(binding = 0) uniform UBO {
	mat4 proj;
	mat4 view;
} ubo;

layout( push_constant ) uniform Constants {
	mat4 model;
	mat4 view;
} constants;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNorm;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec2 fragUV;

void main() {
	mat4 myView = constants.view;
	myView[3] = vec4(0.0, 0.0, 0.0, 1.0);
	vec4 pos = ubo.proj * myView * constants.model * vec4(inPosition, 1.0);
	gl_Position = pos;
	fragUV = inUV;
}
