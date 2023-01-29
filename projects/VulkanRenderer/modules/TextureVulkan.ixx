module;
#include <string>
#include <memory>
#include <vulkan/vulkan.h>
export module TextureVulkan;

import Texture;
import RendererVulkan;

export namespace gg
{
	class TextureVulkan : public Texture
	{
	public:
		TextureVulkan(std::string const& textureRelativePath, std::shared_ptr<RendererVulkan>);
		~TextureVulkan();

		VkImageView textureImageView;
		VkSampler textureSampler;

	private:
		void CreateTextureImage();
		void CreateImage(uint32_t width, uint32_t height, VkFormat, VkImageTiling, VkImageUsageFlags, VkMemoryPropertyFlags, VkImage&, VkDeviceMemory&);
		void CreateTextureImageView();

		void TransitionImageLayout(VkImage, VkFormat, VkImageLayout, VkImageLayout);
		void CopyBufferToImage(VkBuffer, VkImage, uint32_t width, uint32_t height);

		std::weak_ptr<RendererVulkan> rendererWeak;
		VkImage textureImage;
		VkDeviceMemory textureImageMemory;
	};

	VkImageView CreateImageView(VkDevice, VkImage, VkFormat);
}