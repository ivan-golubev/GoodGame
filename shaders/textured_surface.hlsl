#include "common.hlsli"
#include "lighting.hlsli"

VK_BINDING(0) ConstantBuffer<ModelViewProjection> ModelViewProjectionCB : register(b0);
VK_BINDING(1) ConstantBuffer<DirectionalLight> DirectionalLightCB : register(b1);
VK_BINDING(2) Texture2D    texture1 : register(t0);
VK_BINDING(2) SamplerState sampler1 : register(s0);

struct VSInput
{
	float4 position : POSITION;
	float2 texCoord : TEXCOORD;
	float3 normal : NORMAL;
};

struct VSOutput
{
	float2 texCoord : TEXCOORD;
	float3 normal : NORMAL;
	float4 position : SV_Position;
};

VSOutput vs_main(VSInput input)
{
	VSOutput output;
	output.position = mul(ModelViewProjectionCB.MVP, input.position);
	output.texCoord = input.texCoord;

	output.normal = mul(input.normal, (float3x3)ModelViewProjectionCB.MVP);
	output.normal = normalize(output.normal);
	return output;
}

struct PSInput
{
	float2 texCoord : TEXCOORD;
	float3 normal : NORMAL;
};

float4 ps_main(PSInput input) : SV_Target
{
	float4 textureColor = texture1.Sample(sampler1, input.texCoord);
	float4 lightColor = GetLightColor(input.normal, DirectionalLightCB);
	return lightColor * textureColor;
}
