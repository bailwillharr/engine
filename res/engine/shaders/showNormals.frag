#version 450

layout(set = 2, binding = 0) uniform sampler2D materialSetAlbedoSampler;
layout(set = 2, binding = 1) uniform sampler2D materialSetNormalSampler;
layout(set = 2, binding = 2) uniform sampler2D materialSetOcclusionSampler;
layout(set = 2, binding = 3) uniform sampler2D materialSetMetallicRoughnessSampler;

layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec3 fragPosTangentSpace;
layout(location = 2) in vec3 fragViewPosTangentSpace;
layout(location = 3) in vec3 fragLightPosTangentSpace;

layout(location = 0) out vec4 outColor;

void main() {

	vec3 norm = vec3(texture(materialSetNormalSampler, fragUV));
	//norm.y = 1.0 - norm.y;
	norm = normalize(norm * 2.0 - 1.0);
	norm.b = 0.0;
	norm *= 100.0;
	norm = clamp(norm, 0.0, 1.0);
	outColor = vec4(norm, 1.0);
}

