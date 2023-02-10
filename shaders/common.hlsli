#ifndef COMMON_HLSL
#define COMMON_HLSL


#ifdef VULKAN

#define VK_BINDING(N) [[vk::binding(N)]]

#else // !VULKAN

#define VK_BINDING(N)

#endif // !VULKAN


#endif // !COMMON_HLSL
