#version 450

layout( push_constant ) uniform Constants {
	vec4 positions[2];
	vec3 color;
} constants;

layout(location = 0) out vec3 color;

void main() {
	color = constants.color;
	gl_Position = constants.positions[gl_VertexIndex];
	gl_Position.y *= -1.0;
}
