#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec2 fragUV;

layout(set = 0, binding = 0) uniform MyUniform {
	vec4 someValue;
} myUniform;

void main()
{
	gl_Position = vec4(inPosition, 1.0);
	fragUV = inUV * myUniform.someValue.xy;
}
