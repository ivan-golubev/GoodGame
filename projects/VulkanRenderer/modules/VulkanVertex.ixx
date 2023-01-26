module;
#include <array>
#include <vulkan/vulkan.h>
export module VulkanVertex;

export namespace gg
{
	VkVertexInputBindingDescription GetVertexBindingDescription();
	std::array<VkVertexInputAttributeDescription, 2> GetVertexAttributeDescriptions();

} // namespace gg
