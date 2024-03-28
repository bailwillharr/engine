#version 450

layout(set = 2, binding = 0) uniform sampler2D materialSetAlbedoSampler;

layout(location = 0) in vec2 fragUV;

void main() {
	if (texture(materialSetAlbedoSampler, fragUV).a < 1.0) discard;
}
