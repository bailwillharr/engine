#version 450

layout(set = 2, binding = 0) uniform sampler2D materialSetSampler;

layout(location = 0) in vec2 fragUV;

layout(location = 0) out vec4 outColor;

void main() {

	outColor = texture(materialSetSampler, fragUV);
}

