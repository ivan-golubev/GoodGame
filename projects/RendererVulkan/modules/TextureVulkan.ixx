module;
#include <expected>
#include <string>
#include <vulkan/vulkan.h>
export module TextureVulkan;

export import ErrorHandling;
import Texture;

export namespace gg
{
	/* Texels are stored and sampled as-is, without sRGB decoding, matching the UNORM swapchain (and the D3D12 renderer). */
	constexpr VkFormat textureFormat{ VK_FORMAT_R8G8B8A8_UNORM };

	class TextureVulkan : public Texture
	{
	public:
		TextureVulkan(VkDevice, std::string const& textureRelativePath);
		~TextureVulkan() noexcept;
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

	std::expected<VkImageView, Error> CreateImageView(VkDevice, VkImage, VkFormat, VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D, VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT);
}
