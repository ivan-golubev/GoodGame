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
		TextureVulkan(SkyboxTextures const& relativePaths, VkDevice);
		~TextureVulkan() noexcept override;

		VkImageView textureImageView;
		VkSampler textureSampler;

	private:
		TextureVulkan();
		void CreateTextureImage();
		void CreateResources();

		VkDevice device;
		VkImage textureImage;
		VkDeviceMemory textureImageMemory;
	};

	void CreateImage(uint32_t width, uint32_t height, VkFormat, VkImageTiling, VkImageUsageFlags, VkMemoryPropertyFlags, VkImage&, VkDeviceMemory&, uint32_t arrayLayers, VkImageCreateFlags);
	void TransitionImageLayout(VkImage, VkFormat, VkImageLayout, VkImageLayout, uint32_t layerCount);
	void CopyBufferToImage(VkBuffer, VkImage, uint32_t width, uint32_t height, uint32_t layerCount);
	VkImageView CreateImageView(VkDevice, VkImage, VkFormat, uint32_t layerCount = 1, VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D);
}