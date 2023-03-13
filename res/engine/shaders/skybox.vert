#version 450

layout( push_constant ) uniform Constants {
	mat4 model;
	mat4 view;
} constants;

layout(set = 0, binding = 0) uniform SetZeroBuffer {
	mat4 proj;
	vec2 myValue;
} setZeroBuffer;

layout(location = 0) in vec3 inPosition;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec2 fragUV;

void main() {
	mat4 myView = constants.view;
	myView[3] = vec4(0.0, 0.0, 0.0, 1.0);
	vec4 pos = setZeroBuffer.proj * myView * constants.model * vec4(inPosition, 1.0);
	gl_Position = pos;
	fragUV = inUV;
}
