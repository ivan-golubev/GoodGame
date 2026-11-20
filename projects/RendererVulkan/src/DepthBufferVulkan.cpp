module;
#include <expected>
#include <vulkan/vulkan.h>
module DepthBufferVulkan;

import ErrorHandling;
import TextureVulkan;

namespace gg
{
	DepthBufferVulkan::DepthBufferVulkan(VkDevice device, VkPhysicalDevice physicalDevice, VkFormat format)
		: device{ device }
		, physicalDevice{ physicalDevice }
		, format{ format }
	{
	}

	DepthBufferVulkan::~DepthBufferVulkan() noexcept
	{
		Destroy();
	}

	std::expected<VkFormat, Error> DepthBufferVulkan::FindFormat(VkPhysicalDevice physicalDevice)
	{
		/* D32_SFLOAT: full float precision and native on all desktop GPUs (D24 is emulated on some).
		   X8_D24 and D16 are fallbacks; Vulkan guarantees D16_UNORM support. No stencil is needed. */
		constexpr VkFormat candidates[]{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_X8_D24_UNORM_PACK32, VK_FORMAT_D16_UNORM };
		for (VkFormat const candidate : candidates)
		{
			VkFormatProperties properties{};
			vkGetPhysicalDeviceFormatProperties(physicalDevice, candidate, &properties);
			if (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
				return candidate;
		}
		return std::unexpected(Error{ .code = ErrorCode::VulkanRendererInitializationFailed, .message = "No supported Vulkan depth buffer format found." });
	}

	std::expected<void, Error> DepthBufferVulkan::Create(VkExtent2D extent)
	{
		Destroy();

		/* Only used inside the render pass (cleared on load, not stored), so it is a transient attachment:
		   on tile-based GPUs it can live in on-chip memory and never be backed by RAM. */
		VkImageCreateInfo const imageInfo{
			.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			.imageType     = VK_IMAGE_TYPE_2D,
			.format        = format,
			.extent        = { .width = extent.width, .height = extent.height, .depth = 1 },
			.mipLevels     = 1,
			.arrayLayers   = 1,
			.samples       = VK_SAMPLE_COUNT_1_BIT,
			.tiling        = VK_IMAGE_TILING_OPTIMAL,
			.usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
			.sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		};
		if (VK_SUCCESS != vkCreateImage(device, &imageInfo, nullptr, &image))
			return std::unexpected(Error{ .code = ErrorCode::VulkanResourceCreationFailed, .message = "Failed to create Vulkan depth image." });

		VkMemoryRequirements memRequirements{};
		vkGetImageMemoryRequirements(device, image, &memRequirements);
		auto memoryTypeResult{ FindMemoryType(memRequirements.memoryTypeBits) };
		if (!memoryTypeResult)
			return std::unexpected(memoryTypeResult.error());
		VkMemoryAllocateInfo const allocInfo{
			.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.allocationSize  = memRequirements.size,
			.memoryTypeIndex = *memoryTypeResult,
		};
		if (VK_SUCCESS != vkAllocateMemory(device, &allocInfo, nullptr, &memory))
			return std::unexpected(Error{ .code = ErrorCode::VulkanResourceCreationFailed, .message = "Failed to allocate Vulkan depth image memory." });
		if (VK_SUCCESS != vkBindImageMemory(device, image, memory, 0))
			return std::unexpected(Error{ .code = ErrorCode::VulkanResourceCreationFailed, .message = "Failed to bind Vulkan depth image memory." });

		/* No layout transition needed: the render pass takes it from UNDEFINED and clears it every frame. */
		auto viewResult{ CreateImageView(device, image, format, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_DEPTH_BIT) };
		if (!viewResult)
			return std::unexpected(viewResult.error());
		view = *viewResult;
		return {};
	}

	VkFormat    DepthBufferVulkan::GetFormat() const { return format; }
	VkImageView DepthBufferVulkan::GetView() const { return view; }

	void DepthBufferVulkan::Destroy()
	{
		if (!device)
			return;
		if (view) vkDestroyImageView(device, view, nullptr);
		if (image) vkDestroyImage(device, image, nullptr);
		if (memory) vkFreeMemory(device, memory, nullptr);
		view   = VK_NULL_HANDLE;
		image  = VK_NULL_HANDLE;
		memory = VK_NULL_HANDLE;
	}

	std::expected<uint32_t, Error> DepthBufferVulkan::FindMemoryType(uint32_t typeFilter) const
	{
		/* Prefer lazily allocated memory for the transient image (tile-based GPUs); desktop GPUs don't expose it,
		   so fall back to plain device-local memory. */
		VkPhysicalDeviceMemoryProperties memProperties{};
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
		constexpr VkMemoryPropertyFlags preferences[]{
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		};
		for (VkMemoryPropertyFlags const wanted : preferences)
			for (uint32_t i{ 0 }; i < memProperties.memoryTypeCount; ++i)
				if ((typeFilter & (1u << i)) && (memProperties.memoryTypes[i].propertyFlags & wanted) == wanted)
					return i;
		return std::unexpected(Error{ .code = ErrorCode::VulkanResourceCreationFailed, .message = "No suitable memory type for the Vulkan depth buffer." });
	}
} // namespace gg
