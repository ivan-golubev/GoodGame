module;
#include <array>
#include <expected>
#include <memory>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>
export module SkyboxVulkan;

import ErrorHandling;
import GraphicsPipelineVulkan;
import ShaderProgram;
import Texture;

export namespace gg
{
	class SkyboxVulkan
	{
	public:
		SkyboxVulkan(std::string const& name, std::shared_ptr<ShaderProgram>, VkDevice);
		~SkyboxVulkan() noexcept;
		std::expected<void, Error> Initialize();

		VkImageView textureImageView{};
		VkSampler textureSampler{};

		std::shared_ptr<ShaderProgram> shaderProgram;

	private:
		std::expected<void, Error> CreateTextureImage();
		std::expected<void, Error> CreateTextureImageView();
		std::expected<void, Error> TransitionImageLayout(VkImage, VkImageLayout, VkImageLayout);
		std::expected<void, Error> CopyBufferToImage(VkBuffer, uint32_t width, uint32_t height);
		std::expected<void, Error> CreatePipeline();

		std::array<std::unique_ptr<Texture>, 6> textures{};

		VkDevice device{};
		VkImage textureImage{};
		VkDeviceMemory textureImageMemory{};

		GraphicsPipeline graphicsPipeline{};

		friend class RendererVulkan;
	};
}
