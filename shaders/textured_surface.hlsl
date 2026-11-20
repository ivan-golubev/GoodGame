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
	float4 position : SV_Position;
	float4 normal : NORMAL;
	float2 texCoord : TEXCOORD;
};

VSOutput VSMain(VSInput input)
{
	VSOutput output;
	output.position = mul(ModelViewProjectionCB.MVP, input.position);
	output.normal = normalize(mul(ModelViewProjectionCB.MVP, input.normal));
	output.texCoord = input.texCoord;

	return output;
}

struct PSInput
{
	float4 position : SV_Position;
	float4 normal : NORMAL;
	float2 texCoord : TEXCOORD;
};

float4 PSMain(PSInput input) : SV_Target
{
	float4 textureColor = diffuseTexture.Sample(linearSampler, input.texCoord);

	float3 viewDir = (float3) normalize(ModelViewProjectionCB.viewPosition - input.position);
	float4 lightColor = BlinnPhong((float3)input.normal, viewDir, DirectionalLightCB);

	return lightColor * textureColor;
}
