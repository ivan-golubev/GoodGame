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
	float4 ViewPosition;
};

struct ViewProjection
{
	matrix<float, 4, 4> VP;
};

#endif // !COMMON_HLSLI
