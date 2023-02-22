module;
#include <string>
#include <vulkan/vulkan_core.h>
#include <memory>
module SkyboxVulkan;

import RendererVulkan;
import TextureVulkan;

namespace gg
{
	SkyboxVulkan::SkyboxVulkan(std::string const& name, VkDevice device)
		: Skybox{ name }
		, device{ device }
	{
		CreateTextureImage();
		textureImageView = CreateImageView(device, textureImage, VK_FORMAT_R8G8B8A8_SRGB, static_cast<uint32_t>(textures.size()), VK_IMAGE_VIEW_TYPE_CUBE);
		textureSampler = RendererVulkan::Get()->CreateTextureSampler();
	}

	SkyboxVulkan::~SkyboxVulkan() noexcept
	{
		vkDestroySampler(device, textureSampler, nullptr);
		vkDestroyImageView(device, textureImageView, nullptr);
		vkDestroyImage(device, textureImage, nullptr);
		vkFreeMemory(device, textureImageMemory, nullptr);
	}

	void SkyboxVulkan::CreateTextureImage()
	{
		/* image size and layer size */
		VkDeviceSize const imageSizeBytes{ GetDataSizeBytes() };
		VkDeviceSize const layerSize{ GetLayerSizeBytes() };
		uint32_t const imageCount{ static_cast<uint32_t>(textures.size()) };
		uint32_t const width{ textures[0].width };
		uint32_t const height{ textures[0].height };

		std::shared_ptr<RendererVulkan> renderer{ RendererVulkan::Get() };
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		renderer->CreateBuffer(stagingBuffer
			, stagingBufferMemory
			, imageSizeBytes
			, VK_BUFFER_USAGE_TRANSFER_SRC_BIT
			, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
		);

		void* mappedData;
		vkMapMemory(device, stagingBufferMemory, 0, imageSizeBytes, 0, &mappedData);

		for (uint32_t i = 0; i < imageCount; ++i)
			memcpy(static_cast<uint8_t*>(mappedData) + layerSize * i, textures[i].pixels, static_cast<size_t>(layerSize));

		vkUnmapMemory(device, stagingBufferMemory);

		CreateImage(width
			, height
			, VK_FORMAT_R8G8B8A8_SRGB
			, VK_IMAGE_TILING_OPTIMAL
			, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
			, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
			, textureImage
			, textureImageMemory
			, imageCount
			, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT
		);

		TransitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, imageCount);
		CopyBufferToImage(stagingBuffer, textureImage, width, height, imageCount);
		TransitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, imageCount);

		vkDestroyBuffer(device, stagingBuffer, nullptr);
		vkFreeMemory(device, stagingBufferMemory, nullptr);
	}



} // namespace gg
