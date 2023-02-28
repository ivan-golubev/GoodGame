#include "common.hlsli"

VK_BINDING(0) ConstantBuffer<ViewProjection> ViewProjectionCB : register(b0);
VK_BINDING(1) TextureCube  texture0 : register(t0);
VK_BINDING(1) SamplerState sampler0 : register(s0);

struct VSOutput
{
	float4 position : SV_Position;
	float3 texCoord : TEXCOORD;
};

VSOutput vs_main(float3 position : POSITION)
{
	VSOutput output;
	output.position = mul(ViewProjectionCB.VP, float4(position, 1.0));
	output.texCoord = position;
	return output;
}

float4 ps_main(VSOutput input) : SV_Target
{
	return texture0.Sample(sampler0, input.texCoord);
}
