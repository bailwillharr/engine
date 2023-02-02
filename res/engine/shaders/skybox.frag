#version 450

layout(location = 0) in vec2 fragUV;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D texSampler;

void main() {

	gl_FragDepth = 0.9999;
	outColor = texture(texSampler, fragUV);

}

