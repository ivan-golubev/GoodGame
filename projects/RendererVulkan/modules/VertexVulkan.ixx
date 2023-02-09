module;
#include <array>
#include <vulkan/vulkan.h>
export module VertexVulkan;

export namespace gg
{
	VkVertexInputBindingDescription GetVertexBindingDescription();
	std::array<VkVertexInputAttributeDescription, 3> GetVertexAttributeDescriptions();

} // namespace gg
