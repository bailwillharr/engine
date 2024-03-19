#version 450

layout(set = 0, binding = 0) uniform GlobalSetUniformBuffer {
	mat4 proj;
} globalSetUniformBuffer;

layout(set = 1, binding = 0) uniform FrameSetUniformBuffer {
	mat4 view;
} frameSetUniformBuffer;

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec3 fragPosition;

void main() {
	fragPosition = inPosition;
	gl_Position = (globalSetUniformBuffer.proj * vec4(mat3(frameSetUniformBuffer.view) * inPosition, 0.0)).xyzz;

	gl_Position.y *= -1.0;
}
