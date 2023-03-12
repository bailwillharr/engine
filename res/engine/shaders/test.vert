#version 450

layout( push_constant ) uniform Constants {
	mat4 model;
	mat4 view;
} constants;

layout(location = 0) in vec3 inPosition;

void main()
{
	gl_Position = constants.view * constants.model * vec4(inPosition, 1.0);
}
