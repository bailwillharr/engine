#version 450

layout( push_constant ) uniform Constants {
	vec4 positions[2];
} constants;

void main() {
	gl_Position = constants.positions[gl_VertexIndex];
	gl_Position.y *= -1.0;
}
