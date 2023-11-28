#version 450

layout(set = 2, binding = 0) uniform sampler2D materialSetAlbedoSampler;
layout(set = 2, binding = 1) uniform sampler2D materialSetNormalSampler;
layout(set = 2, binding = 2) uniform sampler2D materialSetOcclusionSampler;
layout(set = 2, binding = 3) uniform sampler2D materialSetMetallicRoughnessSampler;

layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec3 fragPosTangentSpace;
layout(location = 2) in vec3 fragViewPosTangentSpace;
layout(location = 3) in vec3 fragLightPosTangentSpace;

layout(location = 0) out vec4 outColor;

void main() {

	// constants
	vec3 lightColor = vec3(1.0, 0.9, 0.9);
	vec3 ambientColor = vec3(1.0, 0.0, 0.0);
	float ambientStrength = 0.05;
	vec3 emission = vec3(0.0, 0.0, 0.0);
	
	vec3 baseColor = vec3(texture(materialSetAlbedoSampler, fragUV));
	
	vec3 norm = vec3(texture(materialSetNormalSampler, fragUV));
	//norm.y = 1.0 - norm.y;
	norm = normalize(norm * 2.0 - 1.0);
	
	vec3 lightDir = normalize(fragLightPosTangentSpace - fragPosTangentSpace);
	vec3 viewDir = normalize(fragViewPosTangentSpace - fragPosTangentSpace);
	
	vec3 diffuse = max(dot(norm, lightDir), 0.0) * lightColor;
	vec3 ambient = ambientColor * ambientStrength;
	vec3 halfwayDir = normalize(lightDir + viewDir);
	float spec = pow(max(dot(norm, halfwayDir), 0.0), 32.0);
	vec3 specular = spec * lightColor;
	
	vec3 lighting = diffuse + ambient + specular;
	outColor = vec4(min(baseColor * (lighting + emission), 1.0), 1.0);
}

