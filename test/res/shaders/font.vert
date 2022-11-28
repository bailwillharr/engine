#version 450

layout( push_constant ) uniform Constants {
	mat4 model;
	vec2 atlas_top_left;
	vec2 atlas_bottom_right;
	vec2 offset;
	vec2 size;
} constants;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNorm;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec2 fragUV;

void main() {
	vec2 position = inPosition.xy * constants.size + constants.offset;
	position *= 0.001;
	gl_Position = constants.model * vec4(position, 0.0, 1.0);
	fragUV = constants.atlas_top_left + (inUV * (constants.atlas_bottom_right - constants.atlas_top_left));
}