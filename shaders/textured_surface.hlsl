#include "common.hlsli"
#include "lighting.hlsli"

VK_BINDING(0) ConstantBuffer<ModelViewProjection> ModelViewProjectionCB : register(b0);
VK_BINDING(1) ConstantBuffer<DirectionalLight> DirectionalLightCB : register(b1);
VK_BINDING(2) Texture2D    texture0 : register(t0);
VK_BINDING(2) SamplerState sampler0 : register(s0);

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

VSOutput vs_main(VSInput input)
{
	VSOutput output;
	output.position = mul(ModelViewProjectionCB.MVP, input.position);
	output.normal = normalize(mul(ModelViewProjectionCB.MVP, input.normal));
	output.texCoord = input.texCoord;
	return output;
}

float4 ps_main(VSOutput input) : SV_Target
{
	float4 textureColor = texture0.Sample(sampler0, input.texCoord);
	float3 viewDir = (float3) normalize(ModelViewProjectionCB.ViewPosition - input.position);
	float4 lightColor = BlinnPhong((float3)input.normal, viewDir, DirectionalLightCB);
	return lightColor * textureColor;
}
