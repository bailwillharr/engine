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
layout(location = 1) in vec3 inNorm;
layout(location = 2) in vec4 inTangent;
layout(location = 3) in vec2 inUV;

layout(location = 0) out vec2 fragUV;

void main() {
	gl_Position = setZeroBuffer.proj * constants.view * constants.model * vec4(inPosition, 1.0);
	fragUV = inUV;
}
