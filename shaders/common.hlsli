#ifndef COMMON_HLSLI
#define COMMON_HLSLI


#ifdef VULKAN

#define VK_BINDING(N) [[vk::binding(N)]]

#else // !VULKAN

#define VK_BINDING(N)

#endif // !VULKAN

struct ModelViewProjection
{
	matrix<float, 4, 4> MVP;
	float4 viewPosition;
};

struct SkyboxCamera
{
	matrix<float, 4, 4> invProjection;   // inverse of the camera projection
	matrix<float, 4, 4> invViewRotation; // camera orientation in world space (inverse view, rotation only)
};

#endif // !COMMON_HLSLI
