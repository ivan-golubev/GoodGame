#include "common.hlsli"
#include "lighting.hlsli"

VK_BINDING(0) ConstantBuffer<ModelViewProjection> ModelViewProjectionCB : register(b0);
VK_BINDING(1) ConstantBuffer<DirectionalLight> DirectionalLightCB : register(b1);
VK_BINDING(2) Texture2D    texture0 : register(t0);
VK_BINDING(2) SamplerState sampler0 : register(s0);

struct VSInput
{
	float4 position : POSITION;
	float2 texCoord : TEXCOORD;
	float3 normal : NORMAL;
};

struct VSOutput
{
	float4 position : SV_Position;
	float3 normal : NORMAL;
	float2 texCoord : TEXCOORD;
};

VSOutput vs_main(VSInput input)
{
	VSOutput output;
	output.position = mul(ModelViewProjectionCB.MVP, input.position);
	output.texCoord = input.texCoord;
	output.normal = normalize(mul((float3x3)ModelViewProjectionCB.NormalMatrix, input.normal));
	return output;
}

struct PSInput
{
	float4 position : SV_Position;
	float3 normal : NORMAL;
	float2 texCoord : TEXCOORD;
};

float4 ps_main(PSInput input) : SV_Target
{
	float4 textureColor = texture0.Sample(sampler0, input.texCoord);

	float3 viewDir = (float3) normalize(ModelViewProjectionCB.ViewPosition - input.position);
	float4 lightColor = BlinnPhong(input.normal, viewDir, DirectionalLightCB);

	return lightColor * textureColor;
}
