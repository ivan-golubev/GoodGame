module;
#include <array>
#include <expected>
#include <memory>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>
export module SkyboxVulkan;

import ErrorHandling;
import Texture;

export namespace gg
{
	class SkyboxVulkan
	{
	public:
		SkyboxVulkan(std::string const& name, VkDevice device);
		~SkyboxVulkan() noexcept;
		std::expected<void, Error> Initialize();

		VkImageView textureImageView{};
		VkSampler textureSampler{};

	private:
		std::expected<void, Error> CreateTextureImage();
		std::expected<void, Error> CreateTextureImageView();
		std::expected<void, Error> TransitionImageLayout(VkImage, VkImageLayout, VkImageLayout);
		std::expected<void, Error> CopyBufferToImage(VkBuffer, uint32_t width, uint32_t height);

		std::array<std::unique_ptr<Texture>, 6> textures{};
		VkDevice device{};
		VkImage textureImage{};
		VkDeviceMemory textureImageMemory{};
	};
}
