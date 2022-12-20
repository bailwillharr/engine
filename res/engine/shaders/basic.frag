#version 450

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNorm;
layout(location = 2) in vec2 fragUV;
layout(location = 3) in vec3 fragLightPos;

layout(location = 0) out vec4 outColor;

void main() {

	// constants
	vec3 lightColor = vec3(1.0, 1.0, 1.0);
	vec3 ambientColor = vec3(1.0, 1.0, 1.0);
	float ambientStrength = 0.05;
	vec3 baseColor = vec3(fragUV.x, 0.0, fragUV.y);
	vec3 emission = vec3(0.0, 0.0, 0.0);
	
	// code

	vec3 norm = normalize(fragNorm);
	vec3 lightDir = normalize(fragLightPos - fragPos);

	vec3 diffuse = max(dot(norm, lightDir), 0.0) * lightColor;
	vec3 ambient = ambientColor * ambientStrength;

	vec3 viewDir = normalize(-fragPos);
	vec3 reflectDir = reflect(-lightDir, norm);

	float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
	vec3 specular = 0.5 * spec * lightColor;
	
	vec3 lighting = min(diffuse + ambient + specular, 1.0);
	outColor = min( ( vec4(baseColor, 1.0) ) * vec4(lighting + emission, 1.0), vec4(1.0));
}

