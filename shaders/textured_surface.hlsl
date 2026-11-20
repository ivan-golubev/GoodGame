#include "common.hlsli"
#include "lighting.hlsli"

VK_BINDING(0) ConstantBuffer<ModelViewProjection> ModelViewProjectionCB : register(b0);
VK_BINDING(1) ConstantBuffer<DirectionalLight> DirectionalLightCB : register(b1);
VK_BINDING(2) Texture2D    diffuseTexture : register(t0);
VK_BINDING(2) SamplerState linearSampler : register(s0);

struct VSInput
{
	float4 position : POSITION;
	float4 normal : NORMAL;
	float2 texCoord : TEXCOORD;
};

struct VSOutput
{
	float4 position : SV_Position;      // clip space, for the rasterizer
	float3 worldPosition : WORLD_POSITION; // world space, for the view direction
	float3 normal : NORMAL;             // world space
	float2 texCoord : TEXCOORD;
};

VSOutput VSMain(VSInput input)
{
	VSOutput output;
	output.position = mul(ModelViewProjectionCB.MVP, input.position);
	output.worldPosition = mul(ModelViewProjectionCB.model, input.position).xyz;
	// The model matrix only rotates and translates, so its 3x3 part transforms normals correctly.
	// With non-uniform scaling this would need the inverse transpose instead.
	output.normal = mul((float3x3)ModelViewProjectionCB.model, input.normal.xyz);
	output.texCoord = input.texCoord;

	return output;
}

float4 PSMain(VSOutput input) : SV_Target
{
	float4 textureColor = diffuseTexture.Sample(linearSampler, input.texCoord);

	// Interpolation shortens normals between vertices, so normalize per pixel.
	float3 normal = normalize(input.normal);
	float3 viewDirection = normalize(ModelViewProjectionCB.viewPosition.xyz - input.worldPosition);
	float4 lightColor = BlinnPhong(normal, viewDirection, DirectionalLightCB);

	return lightColor * textureColor;
}
