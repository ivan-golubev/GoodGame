module;
#include <array>
#include <expected>
#include <memory>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>
export module SkyboxVulkan;

import Camera;
import ErrorHandling;
import GraphicsPipelineVulkan;
import SettingsRenderer;
import ShaderProgram;
import Texture;

export namespace gg
{
	class SkyboxVulkan
	{
	public:
		SkyboxVulkan(std::string const& name, std::shared_ptr<ShaderProgram>, VkDevice);
		~SkyboxVulkan() noexcept;
		std::expected<void, Error> Initialize(VkDescriptorPool);
		std::expected<void, Error> CreatePipeline();
		void Draw(VkCommandBuffer, uint32_t frame, Camera const&);

	private:
		std::expected<void, Error> CreateTextureImage();
		std::expected<void, Error> CreateTextureImageView();
		std::expected<void, Error> TransitionImageLayout(VkImage, VkImageLayout, VkImageLayout);
		std::expected<void, Error> CopyBufferToImage(VkBuffer, uint32_t width, uint32_t height);
		std::expected<void, Error> CreateDescriptorSetLayout();
		std::expected<void, Error> CreateUniformBuffers();
		std::expected<void, Error> CreateDescriptorSets(VkDescriptorPool);
		void DestroyPipeline();
		void UpdateCamera(uint32_t frame, Camera const&);

		std::shared_ptr<ShaderProgram> shaderProgram;

		std::array<std::unique_ptr<Texture>, 6> textures{};

		VkDevice device{};
		VkImage textureImage{};
		VkDeviceMemory textureImageMemory{};
		VkImageView textureImageView{};
		VkSampler textureSampler{};

		VkDescriptorSetLayout descriptorSetLayout{};
		GraphicsPipeline graphicsPipeline{};

		std::array<VkBuffer, maxFramesInFlight> uniformBuffersCamera{};
		std::array<VkDeviceMemory, maxFramesInFlight> uniformBuffersMemoryCamera{};

		std::array<VkDescriptorSet, maxFramesInFlight> descriptorSets{};
	};
}
