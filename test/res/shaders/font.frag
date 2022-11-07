#version 330

uniform vec3 textColor;

uniform sampler2D tex;

in vec2 f_UV;

out vec4 FragColor;

void main()
{
	FragColor = vec4(textColor, texture(tex, f_UV).r);
}
