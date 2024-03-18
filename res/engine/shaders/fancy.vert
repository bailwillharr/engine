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

layout(location = 0) out vec2 fragUV;
layout(location = 1) out vec3 fragPosTangentSpace;
layout(location = 2) out vec3 fragViewPosTangentSpace;
layout(location = 3) out vec3 fragLightPosTangentSpace;

void main() {
	vec4 worldPosition = constants.model * vec4(inPosition, 1.0);
	gl_Position = globalSetUniformBuffer.proj * frameSetUniformBuffer.view * worldPosition;
	
	vec3 T = normalize(vec3(constants.model * vec4(inTangent.xyz, 0.0)));
	vec3 N = normalize(vec3(constants.model * vec4(inNorm, 0.0)));
	vec3 B = cross(T, N) * inTangent.w;
	mat3 worldToTangentSpace = transpose(mat3(T, B, N));
	
	fragUV = inUV;
	fragPosTangentSpace = worldToTangentSpace * vec3(worldPosition);
	fragViewPosTangentSpace = worldToTangentSpace * vec3(inverse(frameSetUniformBuffer.view) * vec4(0.0, 0.0, 0.0, 1.0));
	fragLightPosTangentSpace = worldToTangentSpace * vec3(10000.0, 0000.0, 20000.0);

	gl_Position.y *= -1.0;
}
