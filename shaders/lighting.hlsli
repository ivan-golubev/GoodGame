#ifndef LIGHTING_HLSLI
#define LIGHTING_HLSLI

struct DirectionalLight
{
	float3 specularColor;
	float specularStrength;

	float3 ambientColor;
	float ambientStrength;

	float3 lightDirection;
	float specularShininess;

	float3 diffuseColor;
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
	float3 ambient = dirLight.ambientStrength * dirLight.ambientColor;

	/* specular light */
	float3 reflectDir = reflect(L, N);
	float specularIntensity = pow(max(dot(V, reflectDir), 0.0), dirLight.specularShininess);
	float3 specular = dirLight.specularStrength * specularIntensity * dirLight.specularColor;

	return float4(diffuse + ambient + specular, 1.0);
}

#endif // !LIGHTING_HLSLI
