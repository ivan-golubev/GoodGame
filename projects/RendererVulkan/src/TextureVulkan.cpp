module;
#include <string>
#include <format>
#include <utility>
#include <vulkan/vulkan.h>
module TextureVulkan;

import Texture;
import ErrorHandling;
import ErrorHandlingVulkan;
import RendererVulkan;

namespace gg
{
	TextureVulkan::TextureVulkan(VkDevice d, std::string const& textureRelativePath)
		: Texture{ textureRelativePath }
		, device{ d }
	{
	}

	std::expected<void, Error> TextureVulkan::Initialize()
	{
		if (auto const result{ Texture::Initialize() }; !result)
			return std::unexpected(result.error());
		if (auto const result{ CreateTextureImage() }; !result)
			return std::unexpected(result.error());
		if (auto const result{ CreateTextureImageView() }; !result)
			return std::unexpected(result.error());
		textureSampler = RendererVulkan::Get()->CreateTextureSampler();
		return {};
	}

	TextureVulkan::~TextureVulkan() noexcept
	{
		if (!device)
			return;
		if (textureSampler)
			vkDestroySampler(device, textureSampler, nullptr);
		if (textureImageView)
			vkDestroyImageView(device, textureImageView, nullptr);
		if (textureImage)
			vkDestroyImage(device, textureImage, nullptr);
		if (textureImageMemory)
			vkFreeMemory(device, textureImageMemory, nullptr);
	}


	std::expected<void, Error> TextureVulkan::CreateTextureImage()
	{
		if (!pixels)
			return std::unexpected(Error{
				.code = ErrorCode::TextureLoadFailed,
				.message = "Cannot upload texture to Vulkan because its pixel data is not loaded",
			});

		std::shared_ptr<RendererVulkan> renderer{ RendererVulkan::Get() };
		VkDevice device = renderer->GetDevice();
		struct StagingResources
		{
			VkDevice device;
			VkBuffer buffer{};
			VkDeviceMemory memory{};

			~StagingResources() noexcept
			{
				if (buffer)
					vkDestroyBuffer(device, buffer, nullptr);
				if (memory)
					vkFreeMemory(device, memory, nullptr);
			}
		} staging{ device };
		VkDeviceSize imageSizeBytes = SizeBytes();
		renderer->CreateBuffer(staging.buffer
			, staging.memory
			, imageSizeBytes
			, VK_BUFFER_USAGE_TRANSFER_SRC_BIT
			, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
		);

		void* mappedData{};
		VkResult const mapResult{ vkMapMemory(device, staging.memory, 0, imageSizeBytes, 0, &mappedData) };
		if (mapResult != VK_SUCCESS)
		{
			return std::unexpected(Error{
				.code = ErrorCode::VulkanTextureResourceCreationFailed,
				.message = std::format("Failed to map staging buffer memory for texture upload (VkResult {})", static_cast<int>(mapResult)),
			});
		}
		memcpy(mappedData, pixels, static_cast<size_t>(imageSizeBytes));
		vkUnmapMemory(device, staging.memory);

		auto imageResult{ CreateImage(width
			, height
			, VK_FORMAT_R8G8B8A8_SRGB
			, VK_IMAGE_TILING_OPTIMAL
			, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
			, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
			, textureImage
			, textureImageMemory) };
		if (!imageResult)
			return std::unexpected(imageResult.error());

		auto transitionToTransferResult{ TransitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) };
		if (!transitionToTransferResult)
			return std::unexpected(transitionToTransferResult.error());
		auto copyResult{ CopyBufferToImage(staging.buffer, textureImage, width, height) };
		if (!copyResult)
			return std::unexpected(copyResult.error());
		auto transitionToShaderResult{ TransitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) };
		if (!transitionToShaderResult)
			return std::unexpected(transitionToShaderResult.error());
		return {};
	}

	std::expected<void, Error> TextureVulkan::CreateImage(
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

		VkResult const imageResult{ vkCreateImage(device, &imageInfo, nullptr, &image) };
		if (imageResult != VK_SUCCESS)
			return std::unexpected(Error{
				.code = ErrorCode::VulkanTextureResourceCreationFailed,
				.message = std::format("Failed to create Vulkan texture image (VkResult {})", static_cast<int>(imageResult)),
			});

		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(device, image, &memRequirements);

		VkMemoryAllocateInfo allocInfo{
			.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.allocationSize = memRequirements.size,
			.memoryTypeIndex = renderer->FindMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
		};

		VkResult const allocationResult{ vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) };
		if (allocationResult != VK_SUCCESS)
			return std::unexpected(Error{
				.code = ErrorCode::VulkanTextureResourceCreationFailed,
				.message = std::format("Failed to allocate memory for Vulkan texture image (VkResult {})", static_cast<int>(allocationResult)),
			});

		VkResult const bindResult{ vkBindImageMemory(device, image, imageMemory, 0) };
		if (bindResult != VK_SUCCESS)
			return std::unexpected(Error{
				.code = ErrorCode::VulkanTextureResourceCreationFailed,
				.message = std::format("Failed to bind memory to Vulkan texture image (VkResult {})", static_cast<int>(bindResult)),
			});
		return {};
	}

	std::expected<void, Error> TextureVulkan::TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
	{
		std::shared_ptr<RendererVulkan> renderer{ RendererVulkan::Get() };

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
			return std::unexpected(Error{
				.code = ErrorCode::VulkanTextureUnsupportedLayoutTransition,
				.message = std::format("Cannot transition Vulkan texture image from layout {} to unsupported layout {}", static_cast<int>(oldLayout), static_cast<int>(newLayout)),
			});

		VkCommandBuffer commandBuffer = renderer->BeginSingleTimeCommands();

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
		return {};
	}

	std::expected<void, Error> TextureVulkan::CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
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
		return {};
	}

	std::expected<VkImageView, Error> CreateImageView(VkDevice device, VkImage image, VkFormat format)
	{
		VkImageViewCreateInfo viewInfo{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = image,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = format,
			.subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1 },
		};

		VkImageView imageView{};
		VkResult const result{ vkCreateImageView(device, &viewInfo, nullptr, &imageView) };
		if (result != VK_SUCCESS)
			return std::unexpected(Error{
				.code = ErrorCode::VulkanTextureResourceCreationFailed,
				.message = std::format("Failed to create Vulkan texture image view (VkResult {})", static_cast<int>(result)),
			});
		return imageView;
	}

	std::expected<void, Error> TextureVulkan::CreateTextureImageView()
	{
		auto imageViewResult{ CreateImageView(RendererVulkan::Get()->GetDevice(), textureImage, VK_FORMAT_R8G8B8A8_SRGB) };
		if (!imageViewResult)
			return std::unexpected(imageViewResult.error());
		textureImageView = *imageViewResult;
		return {};
	}
} // namespace gg
