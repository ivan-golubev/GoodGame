#ifndef LIGHTING_HLSLI
#define LIGHTING_HLSLI

struct DirectionalLight
{
	float3 diffuseColor;
	float3 lightDirection;
};

/* N - normalized vertex normal
*  V - normalized view direction
*/
float4 BlinnPhong(float3 N, float3 V, DirectionalLight dirLight)
{
	/* diffuse light */
	float3 L = normalize(-dirLight.lightDirection);
	float lightIntensity = saturate(dot(N, L));
	float3 diffuse = saturate(dirLight.diffuseColor * lightIntensity);

	/* ambient light */
	float ambientStrength = 0.1;
	float3 ambientColor = float3(1.0, 1.0, 1.0);
	float3 ambient = ambientStrength * ambientColor;

	/* specular light */
	float specularStrength = 0.5;
	float3 specularColor = float3(1.0, 1.0, 1.0);
	float shininness = 32;
	float3 reflectDir = reflect(L, N);
	float3 specular = pow(max(dot(V, reflectDir), 0.0), shininness);

	return float4(diffuse + ambient + specular, 1.0);
}

#endif // !LIGHTING_HLSLI
