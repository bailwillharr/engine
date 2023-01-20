#version 450

layout(location = 0) in vec2 fragUV;

layout(location = 0) out vec4 outColor;

void main() {
	vec3 baseColor = vec3(fragUV, 0.0);
	outColor = vec4(baseColor, 1.0);
}

