#include "common.hlsli"

// Skybox drawn as a single fullscreen triangle. No vertex buffer: the vertex shader generates the
// triangle from SV_VertexID, and the pixel shader reconstructs the world-space view ray for each
// pixel and uses it to sample the cube map.

// Camera data for the sky: inverse projection and the camera's rotation (no translation).
VK_BINDING(0) ConstantBuffer<SkyboxCamera> SkyboxCameraCB : register(b0);
// Cube map with its sampler; combined image sampler at binding 2 in Vulkan.
VK_BINDING(2) TextureCube  skyboxTexture : register(t0);
VK_BINDING(2) SamplerState linearSampler : register(s0);

struct VSOutput
{
	float4 position : SV_Position; // clip-space position of the triangle corner
	float2 ndc : TEXCOORD;         // same position in NDC, interpolated so each pixel knows where it is on screen
};

// Called with vkCmdDraw(3, 1, 0, 0): vertexID is 0, 1, 2.
VSOutput vs_main(uint vertexID : SV_VertexID)
{
	// Map the vertex index to a corner without a lookup table:
	// id 0 -> (0, 0), id 1 -> (2, 0), id 2 -> (0, 2).
	float2 uv = float2((vertexID << 1) & 2, vertexID & 2);

	// Remap [0, 2] -> [-1, 3]. The corners (-1,-1), (3,-1), (-1,3) form a triangle that fully contains
	// the [-1, 1] screen square; the parts outside the viewport produce no pixels.
	float2 ndc = uv * 2.0 - 1.0;

	VSOutput output;
	// z = 1: the sky sits on the far plane, behind everything else.
	// w = 1: no perspective divide, so ndc is interpolated linearly and exactly across the screen.
	output.position = float4(ndc, 1.0, 1.0);
	output.ndc = ndc;
	return output;
}

float4 ps_main(VSOutput input) : SV_Target
{
	// Undo the projection: this pixel's screen position on the far plane, back in view space (homogeneous).
	float4 viewPosition = mul(SkyboxCameraCB.invProjection, float4(input.ndc, 1.0, 1.0));

	// Perspective divide gives the view-space point. The camera is at the view-space origin, so this point
	// is also the direction of the ray through the pixel.
	float3 viewDirection = viewPosition.xyz / viewPosition.w;

	// Rotate the ray into world space. Only the 3x3 rotation is used: a direction is unaffected by
	// the camera's position, which keeps the sky fixed as the camera moves.
	float3 worldDirection = mul((float3x3)SkyboxCameraCB.invViewRotation, viewDirection);

	// Cube map lookup by direction; the hardware picks the face from the dominant axis.
	return skyboxTexture.Sample(linearSampler, normalize(worldDirection));
}
