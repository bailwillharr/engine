#version 330

uniform float ambientStrength;
uniform vec3 ambientColor;

uniform vec3 lightColor;
uniform vec3 emission;

uniform vec3 baseColor;
uniform sampler2D tex;

in vec3 f_Pos;
in vec3 f_Norm;
in vec2 f_UV;

in vec3 f_lightPos;

out vec4 FragColor;

void main() {

	vec3 norm = normalize(f_Norm);
	vec3 lightDir = normalize(f_lightPos - f_Pos);

	vec3 diffuse = max(dot(norm, lightDir), 0.0) * lightColor;
	vec3 ambient = ambientColor * ambientStrength;

	vec3 viewDir = normalize(-f_Pos);
	vec3 reflectDir = reflect(-lightDir, norm);

	float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
	vec3 specular = 0.5 * spec * lightColor;
	
	vec3 lighting = min(diffuse + ambient + specular, 1.0);
	FragColor = min( ( vec4(baseColor, 1.0) * texture(tex, f_UV) ) * vec4(lighting + emission, 1.0), vec4(1.0));

}
