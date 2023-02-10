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
	float lightIntensity = saturate(dot(N, -dirLight.lightDirection));
	float4 lightColor = saturate(dirLight.diffuseColor * lightIntensity);
	return lightColor;
}

#endif // !LIGHTING_HLSLI
