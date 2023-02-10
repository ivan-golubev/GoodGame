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
	matrix<float, 4, 4> NormalMatrix;
	float4 ViewPosition;
};

#endif // !COMMON_HLSLI
