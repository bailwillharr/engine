#version 450

layout(location = 0) in vec3 fragNorm;

layout(location = 0) out vec4 outColor;

void main() {
	vec3 baseColor = fragNorm;
	outColor = vec4(baseColor, 1.0);
}

