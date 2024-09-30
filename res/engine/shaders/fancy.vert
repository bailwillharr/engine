#version 450

layout(set = 0, binding = 0) uniform GlobalSetUniformBuffer {
	mat4 proj;
	mat4 lightSpaceMatrix;
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
layout(location = 3) out vec3 fragLightDirTangentSpace;
layout(location = 4) out vec3 fragNormWorldSpace;
layout(location = 5) out vec3 fragViewPosWorldSpace;
layout(location = 6) out vec3 fragPosWorldSpace;
layout(location = 7) out vec4 fragPosLightSpace;
layout(location = 8) out vec4 fragPosScreenSpace;

void main() {
	vec4 worldPosition = constants.model * vec4(inPosition, 1.0);
	gl_Position = globalSetUniformBuffer.proj * frameSetUniformBuffer.view * worldPosition;
	
	mat3 normal_matrix = transpose(inverse(mat3(constants.model)));

	vec3 T = normalize(normal_matrix * inTangent.xyz);
	vec3 N = normalize(normal_matrix * inNorm);
	vec3 B = cross(T, N) * inTangent.w;
	mat3 worldToTangentSpace = transpose(mat3(T, B, N));
	
	fragUV = inUV;
	fragPosTangentSpace = worldToTangentSpace * vec3(worldPosition);
	fragViewPosTangentSpace = worldToTangentSpace * vec3(inverse(frameSetUniformBuffer.view) * vec4(0.0, 0.0, 0.0, 1.0));
	fragLightDirTangentSpace = worldToTangentSpace * vec3(-0.4278,0.7923,0.43502); // directional light

	fragNormWorldSpace = N;
	fragViewPosWorldSpace = vec3(inverse(frameSetUniformBuffer.view) * vec4(0.0, 0.0, 0.0, 1.0));
	fragPosWorldSpace = worldPosition.xyz;

	fragPosLightSpace = globalSetUniformBuffer.lightSpaceMatrix * vec4(worldPosition.xyz, 1.0);
	fragPosScreenSpace = gl_Position;

	gl_Position.y *= -1.0;
}
