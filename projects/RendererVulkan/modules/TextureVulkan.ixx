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
		TextureVulkan(VkDevice, std::string const& textureRelativePath);
		~TextureVulkan();
		void Initialize();

		VkImageView textureImageView{};
		VkSampler textureSampler{};

	private:
		void CreateTextureImage();
		void CreateImage(uint32_t width, uint32_t height, VkFormat, VkImageTiling, VkImageUsageFlags, VkMemoryPropertyFlags, VkImage&, VkDeviceMemory&);
		void CreateTextureImageView();

		void TransitionImageLayout(VkImage, VkFormat, VkImageLayout, VkImageLayout);
		void CopyBufferToImage(VkBuffer, VkImage, uint32_t width, uint32_t height);

		VkDevice device;
		VkImage textureImage{};
		VkDeviceMemory textureImageMemory{};
	};

	VkImageView CreateImageView(VkDevice, VkImage, VkFormat);
}
