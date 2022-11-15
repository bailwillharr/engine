#version 450

layout(binding = 0) uniform UBO {
	mat4 proj;
	vec4 color;
} ubo;

layout( push_constant ) uniform Constants {
	mat4 model;
	mat4 view;
} constants;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNorm;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec3 fragNorm;
layout(location = 2) out vec2 fragUV;
layout(location = 3) out vec3 fragLightPos;
layout(location = 4) out vec3 fragColor;

void main() {
	gl_Position = ubo.proj * constants.view * constants.model * vec4(inPosition, 1.0);
	fragPos = vec3(constants.view * constants.model * vec4(inPosition, 1.0));
	fragNorm = mat3(transpose(inverse(constants.view * constants.model))) * inNorm;
	fragUV = inUV;
	vec3 lightPos = vec3(-5.0, 20.0, 5.0);
	fragLightPos = vec3(constants.view * vec4(lightPos, 1.0));
	fragColor = ubo.color.rgb;
}

