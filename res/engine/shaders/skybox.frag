#version 450

layout(set = 2, binding = 0) uniform sampler2D materialSetAlbedoSampler;
layout(set = 2, binding = 1) uniform sampler2D materialSetNormalSampler;
layout(set = 2, binding = 2) uniform sampler2D materialSetOcclusionSampler;
layout(set = 2, binding = 3) uniform sampler2D materialSetMetallicRoughnessSampler;

layout(location = 0) in vec2 fragUV;

layout(location = 0) out vec4 outColor;

void main() {
	outColor = texture(materialSetAlbedoSampler, fragUV);
}