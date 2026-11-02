module;
#include <string>
#include <vulkan/vulkan.h>
module TextureVulkan;

import Texture;
import ErrorHandling;
import ErrorHandlingVulkan;
import RendererVulkan;

namespace gg
{
	TextureVulkan::TextureVulkan(std::string const& textureRelativePath, VkDevice d)
		: Texture(textureRelativePath)
		, device{ d }
	{
		CreateTextureImage();
		CreateTextureImageView();
		textureSampler = RendererVulkan::Get()->CreateTextureSampler();
	}

	TextureVulkan::~TextureVulkan()
	{
		vkDestroySampler(device, textureSampler, nullptr);
		vkDestroyImageView(device, textureImageView, nullptr);
		vkDestroyImage(device, textureImage, nullptr);
		vkFreeMemory(device, textureImageMemory, nullptr);
	}


	void TextureVulkan::CreateTextureImage()
	{
		if (!pixels)
			throw AssetLoadException("failed to load texture image!");

		std::shared_ptr<RendererVulkan> renderer{ RendererVulkan::Get() };
		VkDevice device = renderer->GetDevice();
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		VkDeviceSize imageSizeBytes = SizeBytes();
		renderer->CreateBuffer(stagingBuffer
			, stagingBufferMemory
			, imageSizeBytes
			, VK_BUFFER_USAGE_TRANSFER_SRC_BIT
			, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
		);

		void* mappedData;
		vkMapMemory(device, stagingBufferMemory, 0, imageSizeBytes, 0, &mappedData);
		memcpy(mappedData, pixels, static_cast<size_t>(imageSizeBytes));
		vkUnmapMemory(device, stagingBufferMemory);

		CreateImage(width
			, height
			, VK_FORMAT_R8G8B8A8_SRGB
			, VK_IMAGE_TILING_OPTIMAL
			, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
			, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
			, textureImage
			, textureImageMemory);

		TransitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		CopyBufferToImage(stagingBuffer, textureImage, width, height);
		TransitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		vkDestroyBuffer(device, stagingBuffer, nullptr);
		vkFreeMemory(device, stagingBufferMemory, nullptr);
	}

	void TextureVulkan::CreateImage(
		uint32_t width
		, uint32_t height
		, VkFormat format
		, VkImageTiling tiling
		, VkImageUsageFlags usage
		, VkMemoryPropertyFlags properties
		, VkImage& image
		, VkDeviceMemory& imageMemory)
	{
		std::shared_ptr<RendererVulkan> renderer{ RendererVulkan::Get() };
		VkDevice device = renderer->GetDevice();

		VkImageCreateInfo imageInfo{
			.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			.imageType = VK_IMAGE_TYPE_2D,
			.format = format,
			.extent = { .width = static_cast<uint32_t>(width), .height = static_cast<uint32_t>(height), .depth = 1 },
			.mipLevels = 1,
			.arrayLayers = 1,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.tiling = VK_IMAGE_TILING_OPTIMAL,
			.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		};

		if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS)
			throw VulkanInitException("failed to create image!");

		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(device, image, &memRequirements);

		VkMemoryAllocateInfo allocInfo{
			.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.allocationSize = memRequirements.size,
			.memoryTypeIndex = renderer->FindMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
		};

		if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS)
			throw VulkanInitException("failed to allocate image memory!");

		vkBindImageMemory(device, image, imageMemory, 0);
	}

	void TextureVulkan::TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
	{
		std::shared_ptr<RendererVulkan> renderer{ RendererVulkan::Get() };
		VkCommandBuffer commandBuffer = renderer->BeginSingleTimeCommands();

		VkPipelineStageFlags sourceStage;
		VkPipelineStageFlags destinationStage;
		VkAccessFlags sourceAccessMask{};
		VkAccessFlags destinationAccessMask{};

		if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
		{
			sourceAccessMask = 0;
			destinationAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		{
			sourceAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			destinationAccessMask = VK_ACCESS_SHADER_READ_BIT;

			sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}
		else
			throw VulkanRenderException("unsupported layout transition!");

		VkImageMemoryBarrier barrier{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcAccessMask = sourceAccessMask,
			.dstAccessMask = destinationAccessMask,
			.oldLayout = oldLayout,
			.newLayout = newLayout,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = image,
			.subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1 },
		};

		vkCmdPipelineBarrier(
			commandBuffer,
			sourceStage, destinationStage,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);

		renderer->EndSingleTimeCommands(commandBuffer);
	}

	void TextureVulkan::CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
	{
		std::shared_ptr<RendererVulkan> renderer{ RendererVulkan::Get() };
		VkCommandBuffer commandBuffer = renderer->BeginSingleTimeCommands();

		VkBufferImageCopy region{
			.bufferOffset = 0,
			.bufferRowLength = 0,
			.bufferImageHeight = 0,
			.imageSubresource = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1 },
			.imageOffset = { .x = 0, .y = 0, .z = 0 },
			.imageExtent = { .width = width, .height = height, .depth = 1 },
		};

		vkCmdCopyBufferToImage(
			commandBuffer,
			buffer,
			image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&region
		);

		renderer->EndSingleTimeCommands(commandBuffer);
	}

	VkImageView CreateImageView(VkDevice device, VkImage image, VkFormat format)
	{
		VkImageViewCreateInfo viewInfo{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = image,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = format,
			.subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1 },
		};

		VkImageView imageView;
		if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS)
			throw VulkanInitException("failed to create texture image view!");
		return imageView;
	}

	void TextureVulkan::CreateTextureImageView()
	{
		textureImageView = CreateImageView(RendererVulkan::Get()->GetDevice(), textureImage, VK_FORMAT_R8G8B8A8_SRGB);
	}
} // namespace gg
