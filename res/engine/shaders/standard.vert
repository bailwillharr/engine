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
layout(location = 1) in vec3 inNorm;
layout(location = 2) in vec4 inTangent;
layout(location = 3) in vec2 inUV;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec3 fragNorm;
layout(location = 2) out vec2 fragUV;
layout(location = 3) out vec3 fragLightPos;

void main() {
	gl_Position = globalSetUniformBuffer.proj * frameSetUniformBuffer.view * constants.model * vec4(inPosition, 1.0);

	fragPos = vec3(frameSetUniformBuffer.view * constants.model * vec4(inPosition, 1.0));
	fragNorm = mat3(transpose(inverse(frameSetUniformBuffer.view * constants.model))) * inNorm;
	fragUV = inUV;

	vec3 lightPos = vec3(2000.0, -2000.0, 2000.0);
	fragLightPos = vec3(frameSetUniformBuffer.view * vec4(lightPos, 1.0));

  gl_Position.y *= -1.0;
}
