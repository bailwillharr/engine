#version 450

#define PI 3.1415926535897932384626433832795
#define PI_INV 0.31830988618379067153776752674503

layout(set = 0, binding = 1) uniform samplerCube globalSetSkybox;
layout(set = 0, binding = 2) uniform sampler2D globalSetShadowmap;

layout(set = 2, binding = 0) uniform sampler2D materialSetAlbedoSampler;
layout(set = 2, binding = 1) uniform sampler2D materialSetNormalSampler;
layout(set = 2, binding = 2) uniform sampler2D materialSetOcclusionRoughnessMetallic;

layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec3 fragPosTangentSpace;
layout(location = 2) in vec3 fragViewPosTangentSpace;
layout(location = 3) in vec3 fragLightPosTangentSpace;
layout(location = 4) in vec3 fragNormWorldSpace;
layout(location = 5) in vec3 fragViewPosWorldSpace;
layout(location = 6) in vec3 fragPosWorldSpace;
layout(location = 7) in vec4 fragPosLightSpace;

layout(location = 0) out vec4 outColor;

vec3 GetNormal() {
	const vec3 norm_colour = vec3(texture(materialSetNormalSampler, fragUV));
	return normalize(norm_colour * 2.0 - 1.0);
}

float GGXDist(float alpha_2, float N_dot_H) {
	const float num = alpha_2 * max(N_dot_H, 0.0);
	const float den = PI * pow(N_dot_H * N_dot_H * (alpha_2 - 1) + 1, 2.0);
	return num / den;
}

void main() {

	const vec3 occlusion_roughness_metallic = vec3(texture(materialSetOcclusionRoughnessMetallic, fragUV));
	const float ao = occlusion_roughness_metallic.r;
	const float roughness = occlusion_roughness_metallic.g;
	const float metallic = occlusion_roughness_metallic.b;

	const vec3 I = normalize(fragPosWorldSpace - fragViewPosWorldSpace);
    const vec3 R = reflect(I, fragNormWorldSpace);

	const float roughness_2 = roughness * roughness;

	vec3 light_colour = vec3(1.0, 1.0, 1.0) * 2.4;
	float light_distance    = length(fragLightPosTangentSpace - fragPosTangentSpace);
	float attenuation = 1.0 / (1.0 + 0.09 * light_distance + 
    		    0.032 * (light_distance * light_distance));  
	//light_colour *= 5.0 * attenuation;

	const vec3 emission = vec3(0.0, 0.0, 0.0);

	const vec3 albedo = vec3(texture(materialSetAlbedoSampler, fragUV));
	const vec3 N = GetNormal();

	const vec3 V = normalize(fragViewPosTangentSpace - fragPosTangentSpace);
	const vec3 L = normalize(fragLightPosTangentSpace /* - fragPosTangentSpace */ );
	//const vec3 L = normalize(vec3(5.0, 0.0, 3.0));
	const vec3 H = normalize(V + L);

	//const vec3 dielectric_brdf = FresnelMix();

	//const vec3 brdf = mix(dielectric_brdf, metal_brdf, metallic);

	const float L_dot_N = max(dot(L, N), 0.000001);
	const float L_dot_H = max(dot(L, H), 0.000001);
	const float V_dot_H = max(dot(V, H), 0.000001);
	const float V_dot_N = max(dot(V, N), 0.000001);
	const float N_dot_H = max(dot(N, H), 0.000001);

	const float vis = ( max(L_dot_H, 0.0) / ( L_dot_N + sqrt(roughness_2 + (1 - roughness_2) * L_dot_N * L_dot_N) ) ) *
	( max(V_dot_H, 0.0) / ( V_dot_N + sqrt(roughness_2 + (1 - roughness_2) * V_dot_N * V_dot_N) ) );

	const vec3 diffuse_brdf = albedo * PI_INV;

	const vec3 specular_brdf = vec3(vis * GGXDist(roughness_2, N_dot_H));

	const vec3 dielectric_brdf = mix(diffuse_brdf, specular_brdf, 0.04 + (1 - 0.04) * pow(1 - abs(V_dot_H), 5));

	const vec3 metal_brdf = specular_brdf * (albedo + (1 - albedo) * pow(1 - V_dot_H, 5.0) );
	
	const vec3 brdf = mix(dielectric_brdf, metal_brdf, metallic);
	
	vec3 lighting = brdf * light_colour * L_dot_N;

	// find if fragment is in shadow
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
	projCoords.x = projCoords.x * 0.5 + 0.5; 
	projCoords.y = projCoords.y * 0.5 + 0.5; 
	//float closestDepth = texture(globalSetShadowmap, projCoords.xy).r;
	const float currentDepth = max(projCoords.z, 0.0);
	const float bias = max(0.02 * (1.0 - L_dot_N), 0.005);  
	//float shadow = currentDepth - bias > closestDepth ? 0.0 : 1.0;
	float shadow = 0.0;
	vec2 texelSize = 1.0 / textureSize(globalSetShadowmap, 0);
	for(int x = -1; x <= 1; ++x)
	{
		for(int y = -1; y <= 1; ++y)
		{
			float pcfDepth = texture(globalSetShadowmap, projCoords.xy + vec2(x, y) * texelSize).r;
			shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;        
		}    
	}
	shadow /= 9.0;

	lighting *= (1.0 - shadow);

	vec3 ambient_light = vec3(0.09082, 0.13281, 0.18164) * 2.4 * 4.0;
	lighting += mix(ambient_light, texture(globalSetSkybox, R).rgb, metallic) * ao * diffuse_brdf; // this is NOT physically-based, it just looks cool

	outColor = vec4(min(emission + lighting, 1.0), 1.0);
	//outColor = vec4(vec3(shadow), 1.0);
}
