#version 450

layout(set = 0, binding = 0) uniform GlobalSetUniformBuffer {
	mat4 proj;
	mat4 lightSpaceMatrix;
} globalSetUniformBuffer;

layout( push_constant ) uniform Constants {
	mat4 model;
} constants;

layout(location = 0) in vec3 inPosition;

void main() {
	gl_Position = globalSetUniformBuffer.lightSpaceMatrix * constants.model * vec4(inPosition, 1.0);
	//gl_Position.y *= -1.0;
}
