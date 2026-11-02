module;
#include <array>
#include <vulkan/vulkan.h>
module VertexVulkan;

import Vertex;
import Model;

namespace gg
{
	VkVertexInputBindingDescription GetVertexBindingDescription()
	{
		VkVertexInputBindingDescription bindingDescription{
			.binding = 0,
			.stride = sizeof(Vertex),
			.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
		};
		return bindingDescription;
	}

	std::array<VkVertexInputAttributeDescription, 3> GetVertexAttributeDescriptions()
	{
		std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{
			VkVertexInputAttributeDescription{ .location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = offsetof(Vertex, position) },
			VkVertexInputAttributeDescription{ .location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = offsetof(Vertex, normal) },
			VkVertexInputAttributeDescription{ .location = 2, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(Vertex, textureCoords0) },
		};

		return attributeDescriptions;
	}

} // namespace gg
