#version 330

uniform vec2 windowSize;
uniform bool textScaling; // true means keep text aspect ratio

layout (location = 0) in vec3 v_Position;
layout (location = 2) in vec2 v_UV;

uniform mat4 modelMat;

out vec2 f_UV;

void main()
{
	float aspect = windowSize.y / windowSize.x;
	
	vec3 pos = v_Position;

	if (textScaling) {
		pos.x *= aspect;
	}

	gl_Position = modelMat * vec4(pos, 1.0);
	f_UV = v_UV;
}
