module;
#include <cstdint>
#include <cstring>
#include <format>
#include <vulkan/vulkan.h>
module SkyboxVulkan;

import ErrorHandling;
import RendererVulkan;
import SettingsRenderer;
import Texture;
import TextureVulkan;

namespace gg
{
	namespace
	{
		constexpr std::array<char const*, 6> skyboxFaceNames{ "left", "top", "front", "bottom", "right", "back" };
	}

	SkyboxVulkan::SkyboxVulkan(std::string const& name, VkDevice device)
		: device{ device }
	{
		for (size_t i{}; i < textures.size(); ++i)
			textures[i] = std::make_unique<Texture>(std::format("{}/skybox/{}/{}.{}", texturesLocation, name, skyboxFaceNames[i], texturesExtension));
	}

	SkyboxVulkan::~SkyboxVulkan() noexcept
	{
		if (!device)
			return;
		if (textureSampler) vkDestroySampler(device, textureSampler, nullptr);
		if (textureImageView) vkDestroyImageView(device, textureImageView, nullptr);
		if (textureImage) vkDestroyImage(device, textureImage, nullptr);
		if (textureImageMemory) vkFreeMemory(device, textureImageMemory, nullptr);
	}

	std::expected<void, Error> SkyboxVulkan::Initialize()
	{
		for (auto& texture : textures)
		{
			if (auto result{ texture->Initialize() }; !result)
				return std::unexpected(result.error());
		}
		for (size_t i{ 1 }; i < textures.size(); ++i)
		{
			if (textures[i]->width != textures[0]->width || textures[i]->height != textures[0]->height)
				return std::unexpected(Error{ .code = ErrorCode::TextureLoadFailed, .message = "Skybox faces must all have the same dimensions." });
		}
		if (textures[0]->width != textures[0]->height)
			return std::unexpected(Error{ .code = ErrorCode::TextureLoadFailed, .message = "Skybox face textures must be square." });

		if (auto result{ CreateTextureImage() }; !result)
			return std::unexpected(result.error());
		if (auto result{ CreateTextureImageView() }; !result)
			return std::unexpected(result.error());
		auto samplerResult{ RendererVulkan::Get()->CreateTextureSampler() };
		if (!samplerResult)
			return std::unexpected(samplerResult.error());
		textureSampler = *samplerResult;
		return {};
	}

	std::expected<void, Error> SkyboxVulkan::CreateTextureImage()
	{
		std::shared_ptr<RendererVulkan> renderer{ RendererVulkan::Get() };
		VkDeviceSize const layerSize{ textures[0]->SizeBytes() };
		VkDeviceSize const imageSize{ layerSize * textures.size() };
		struct StagingResources
		{
			VkDevice device;
			VkBuffer buffer{};
			VkDeviceMemory memory{};
			~StagingResources() noexcept
			{
				if (buffer) vkDestroyBuffer(device, buffer, nullptr);
				if (memory) vkFreeMemory(device, memory, nullptr);
			}
		} staging{ device };
		if (auto result{ renderer->CreateBuffer(staging.buffer, staging.memory, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) }; !result)
			return std::unexpected(result.error());

		void* mappedData{};
		VkResult const mapResult{ vkMapMemory(device, staging.memory, 0, imageSize, 0, &mappedData) };
		if (mapResult != VK_SUCCESS)
			return std::unexpected(Error{ .code = ErrorCode::VulkanTextureResourceCreationFailed, .message = std::format("Failed to map Vulkan skybox staging buffer (VkResult {}).", static_cast<int>(mapResult)) });
		for (size_t i{}; i < textures.size(); ++i)
			std::memcpy(static_cast<uint8_t*>(mappedData) + layerSize * i, textures[i]->pixels, static_cast<size_t>(layerSize));
		vkUnmapMemory(device, staging.memory);

		VkImageCreateInfo imageInfo{
			.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
			.imageType = VK_IMAGE_TYPE_2D,
			.format = VK_FORMAT_R8G8B8A8_SRGB,
			.extent = { .width = textures[0]->width, .height = textures[0]->height, .depth = 1 },
			.mipLevels = 1,
			.arrayLayers = static_cast<uint32_t>(textures.size()),
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.tiling = VK_IMAGE_TILING_OPTIMAL,
			.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		};
		VkResult const createResult{ vkCreateImage(device, &imageInfo, nullptr, &textureImage) };
		if (createResult != VK_SUCCESS)
			return std::unexpected(Error{ .code = ErrorCode::VulkanTextureResourceCreationFailed, .message = std::format("Failed to create Vulkan cubemap image (VkResult {}).", static_cast<int>(createResult)) });

		VkMemoryRequirements requirements{};
		vkGetImageMemoryRequirements(device, textureImage, &requirements);
		auto memoryTypeResult{ renderer->FindMemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) };
		if (!memoryTypeResult)
			return std::unexpected(memoryTypeResult.error());
		VkMemoryAllocateInfo allocationInfo{
			.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.allocationSize = requirements.size,
			.memoryTypeIndex = *memoryTypeResult,
		};
		VkResult const allocateResult{ vkAllocateMemory(device, &allocationInfo, nullptr, &textureImageMemory) };
		if (allocateResult != VK_SUCCESS)
			return std::unexpected(Error{ .code = ErrorCode::VulkanTextureResourceCreationFailed, .message = std::format("Failed to allocate Vulkan cubemap memory (VkResult {}).", static_cast<int>(allocateResult)) });
		VkResult const bindResult{ vkBindImageMemory(device, textureImage, textureImageMemory, 0) };
		if (bindResult != VK_SUCCESS)
			return std::unexpected(Error{ .code = ErrorCode::VulkanTextureResourceCreationFailed, .message = std::format("Failed to bind Vulkan cubemap memory (VkResult {}).", static_cast<int>(bindResult)) });

		if (auto result{ TransitionImageLayout(textureImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) }; !result)
			return std::unexpected(result.error());
		if (auto result{ CopyBufferToImage(staging.buffer, textures[0]->width, textures[0]->height) }; !result)
			return std::unexpected(result.error());
		return TransitionImageLayout(textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	}

	std::expected<void, Error> SkyboxVulkan::TransitionImageLayout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout)
	{
		VkPipelineStageFlags sourceStage{ VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT };
		VkPipelineStageFlags destinationStage{ VK_PIPELINE_STAGE_TRANSFER_BIT };
		VkAccessFlags sourceAccess{};
		VkAccessFlags destinationAccess{ VK_ACCESS_TRANSFER_WRITE_BIT };
		if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		{
			sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			sourceAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
			destinationAccess = VK_ACCESS_SHADER_READ_BIT;
		}
		auto commandResult{ RendererVulkan::Get()->BeginSingleTimeCommands() };
		if (!commandResult) return std::unexpected(commandResult.error());
		VkImageMemoryBarrier barrier{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcAccessMask = sourceAccess,
			.dstAccessMask = destinationAccess,
			.oldLayout = oldLayout,
			.newLayout = newLayout,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = image,
			.subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = static_cast<uint32_t>(textures.size()) },
		};
		vkCmdPipelineBarrier(*commandResult, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
		return RendererVulkan::Get()->EndSingleTimeCommands(*commandResult);
	}

	std::expected<void, Error> SkyboxVulkan::CopyBufferToImage(VkBuffer buffer, uint32_t width, uint32_t height)
	{
		auto commandResult{ RendererVulkan::Get()->BeginSingleTimeCommands() };
		if (!commandResult) return std::unexpected(commandResult.error());
		std::array<VkBufferImageCopy, 6> regions{};
		VkDeviceSize layerSize{ textures[0]->SizeBytes() };
		for (uint32_t i{}; i < regions.size(); ++i)
		{
			regions[i] = VkBufferImageCopy{
				.bufferOffset = layerSize * i,
				.imageSubresource = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = i, .layerCount = 1 },
				.imageExtent = { .width = width, .height = height, .depth = 1 },
			};
		}
		vkCmdCopyBufferToImage(*commandResult, buffer, textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<uint32_t>(regions.size()), regions.data());
		return RendererVulkan::Get()->EndSingleTimeCommands(*commandResult);
	}

	std::expected<void, Error> SkyboxVulkan::CreateTextureImageView()
	{
		VkImageViewCreateInfo viewInfo{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = textureImage,
			.viewType = VK_IMAGE_VIEW_TYPE_CUBE,
			.format = VK_FORMAT_R8G8B8A8_SRGB,
			.subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = static_cast<uint32_t>(textures.size()) },
		};
		VkResult const result{ vkCreateImageView(device, &viewInfo, nullptr, &textureImageView) };
		if (result != VK_SUCCESS)
			return std::unexpected(Error{ .code = ErrorCode::VulkanTextureResourceCreationFailed, .message = std::format("Failed to create Vulkan cubemap image view (VkResult {}).", static_cast<int>(result)) });
		return {};
	}
}
