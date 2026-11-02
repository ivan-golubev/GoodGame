module;
#include <string>
#include <expected>
#include <vulkan/vulkan.h>
export module TextureVulkan;

export import ErrorHandling;
import Texture;

export namespace gg
{
	class TextureVulkan : public Texture
	{
	public:
		TextureVulkan(VkDevice, std::string const& textureRelativePath);
		~TextureVulkan();
		std::expected<void, Error> Initialize();

		VkImageView textureImageView{};
		VkSampler textureSampler{};

	private:
		std::expected<void, Error> CreateTextureImage();
		std::expected<void, Error> CreateImage(uint32_t width, uint32_t height, VkFormat, VkImageTiling, VkImageUsageFlags, VkMemoryPropertyFlags, VkImage&, VkDeviceMemory&);
		std::expected<void, Error> CreateTextureImageView();

		std::expected<void, Error> TransitionImageLayout(VkImage, VkFormat, VkImageLayout, VkImageLayout);
		std::expected<void, Error> CopyBufferToImage(VkBuffer, VkImage, uint32_t width, uint32_t height);

		VkDevice device;
		VkImage textureImage{};
		VkDeviceMemory textureImageMemory{};
	};

	std::expected<VkImageView, Error> CreateImageView(VkDevice, VkImage, VkFormat);
}
