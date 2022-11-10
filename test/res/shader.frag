#version 450

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNorm;
layout(location = 2) in vec2 fragUV;
layout(location = 3) in vec3 fragLightPos;
layout(location = 4) in vec3 fragColor;

layout(location = 0) out vec4 outColor;

float snoise(vec2 v);

void main() {

	// constants
	vec3 lightColor = vec3(1.0, 1.0, 1.0);
	vec3 ambientColor = vec3(1.0, 1.0, 1.0);
	float ambientStrength = 0.1;
	vec3 baseColor = vec3(fragColor.x * snoise(fragUV * 10.0), mod(fragUV.x, 1.0), mod(fragUV.y, 1.0));
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



// Simplex 2D noise
//
vec3 permute(vec3 x) { return mod(((x*34.0)+1.0)*x, 289.0); }

float snoise(vec2 v){
  const vec4 C = vec4(0.211324865405187, 0.366025403784439,
           -0.577350269189626, 0.024390243902439);
  vec2 i  = floor(v + dot(v, C.yy) );
  vec2 x0 = v -   i + dot(i, C.xx);
  vec2 i1;
  i1 = (x0.x > x0.y) ? vec2(1.0, 0.0) : vec2(0.0, 1.0);
  vec4 x12 = x0.xyxy + C.xxzz;
  x12.xy -= i1;
  i = mod(i, 289.0);
  vec3 p = permute( permute( i.y + vec3(0.0, i1.y, 1.0 ))
  + i.x + vec3(0.0, i1.x, 1.0 ));
  vec3 m = max(0.5 - vec3(dot(x0,x0), dot(x12.xy,x12.xy),
    dot(x12.zw,x12.zw)), 0.0);
  m = m*m ;
  m = m*m ;
  vec3 x = 2.0 * fract(p * C.www) - 1.0;
  vec3 h = abs(x) - 0.5;
  vec3 ox = floor(x + 0.5);
  vec3 a0 = x - ox;
  m *= 1.79284291400159 - 0.85373472095314 * ( a0*a0 + h*h );
  vec3 g;
  g.x  = a0.x  * x0.x  + h.x  * x0.y;
  g.yz = a0.yz * x12.xz + h.yz * x12.yw;
  return 130.0 * dot(m, g);
}