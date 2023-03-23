#version 450

layout(set = 0, binding = 0) uniform GlobalSetUniformBuffer {
	mat4 proj;
} globalSetUniformBuffer;

layout(set = 1, binding = 0) uniform FrameSetUniformBuffer {
	mat4 view;
} frameSetUniformBuffer;

layout( push_constant ) uniform Constants {
	mat4 model;
} constants;

layout(location = 0) in vec3 inPosition;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec2 fragUV;

void main() {
	vec3 position = mat3(frameSetUniformBuffer.view) * vec3(constants.model * vec4(inPosition, 1.0));
	gl_Position = (globalSetUniformBuffer.proj * vec4(position, 0.0)).xyzz;
	fragUV = inUV;
}
