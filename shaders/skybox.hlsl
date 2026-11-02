#include "common.hlsli"

VK_BINDING(0) ConstantBuffer<ViewProjection> ViewProjectionCB : register(b0);
VK_BINDING(2) TextureCube  texture0 : register(t0);
VK_BINDING(2) SamplerState sampler0 : register(s0);

struct VSOutput
{
	float4 position : SV_Position;
	float3 direction : TEXCOORD;
};

VSOutput vs_main(float3 position : POSITION)
{
	float4 worldPos = mul(ViewProjectionCB.VP, float4(position, 1.0));

	VSOutput output;
	output.position = worldPos;
	output.direction = normalize(position.xyz);
	return output;
}

float4 ps_main(VSOutput input) : SV_Target
{
	return texture0.Sample(sampler0, input.direction);
}
