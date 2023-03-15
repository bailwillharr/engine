#version 450

layout( push_constant ) uniform Constants {
	mat4 model;
} constants;

layout(set = 0, binding = 0) uniform SetZeroBuffer {
	mat4 proj;
} setZeroBuffer;

layout(set = 1, binding = 0) uniform SetOneBuffer {
	mat4 view;
} setOneBuffer;

layout(location = 0) in vec3 inPosition;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec2 fragUV;

void main() {
	mat4 myView = setOneBuffer.view;
	myView[3] = vec4(0.0, 0.0, 0.0, 1.0);
	vec4 pos = setZeroBuffer.proj * myView * constants.model * vec4(inPosition, 1.0);
	gl_Position = pos;
	fragUV = inUV;
}
