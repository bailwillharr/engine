#version 450

layout( push_constant ) uniform Constants {
	mat4 model;
} constants;

layout(set = 0, binding = 0) uniform SetZeroBuffer {
	mat4 proj;
} setZeroBuffer;

layout(set = 1, binding = 0) uniform SetOneBuffer {
	mat4 view;
} setOneBuffer;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNorm;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec3 fragNorm;
layout(location = 2) out vec2 fragUV;
layout(location = 3) out vec3 fragLightPos;

void main() {
	gl_Position = setZeroBuffer.proj * setOneBuffer.view * constants.model * vec4(inPosition, 1.0);

	fragPos = vec3(setOneBuffer.view * constants.model * vec4(inPosition, 1.0));
	fragNorm = mat3(transpose(inverse(setOneBuffer.view * constants.model))) * inNorm;
	fragUV = inUV;

	vec3 lightPos = vec3(2000.0, 2000.0, -2000.0);
	fragLightPos = vec3(setOneBuffer.view * vec4(lightPos, 1.0));
}
