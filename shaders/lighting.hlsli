#ifndef LIGHTING_HLSLI
#define LIGHTING_HLSLI

struct DirectionalLight
{
	float4 diffuseColor;
	float3 lightDirection;
};

/* N - vertex normal */
float4 GetLightColor(float3 N, DirectionalLight dirLight)
{
	float3 L = normalize(-dirLight.lightDirection);
	float lightIntensity = saturate(dot(N, L));
	float4 lightColor = saturate(dirLight.diffuseColor * lightIntensity);

	/* add ambient light */
	float ambientStrength = 0.1;
	float4 ambientColor = float4(1.0, 1.0, 1.0, 1.0);
	float4 ambient = ambientStrength * ambientColor;

	return lightColor + ambient;
}

#endif // !LIGHTING_HLSLI
