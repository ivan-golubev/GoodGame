module;
#include <string>
#include <vulkan/vulkan.h>
export module TextureVulkan;

import Texture;

export namespace gg
{
	class TextureVulkan : public Texture
	{
	public:
		TextureVulkan(std::string const& textureRelativePath, VkDevice);
		~TextureVulkan() noexcept override;

		VkImageView textureImageView;
		VkSampler textureSampler;

	private:
		void CreateTextureImage();

		VkDevice device;
		VkImage textureImage;
		VkDeviceMemory textureImageMemory;
	};

	void CreateImage(uint32_t width, uint32_t height, VkFormat, VkImageTiling, VkImageUsageFlags, VkMemoryPropertyFlags, VkImage&, VkDeviceMemory&, uint32_t arrayLayers = 1, VkImageCreateFlags = 0);
	void TransitionImageLayout(VkImage, VkFormat, VkImageLayout, VkImageLayout, uint32_t layerCount = 1);
	void CopyBufferToImage(VkBuffer, VkImage, uint32_t width, uint32_t height, uint32_t layerCount = 1);
	VkImageView CreateImageView(VkDevice, VkImage, VkFormat, uint32_t layerCount = 1, VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D);
}