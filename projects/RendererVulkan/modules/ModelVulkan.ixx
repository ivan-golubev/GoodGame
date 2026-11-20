module;
#include <array>
#include <expected>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <memory>
#include <string>
#include <vulkan/vulkan.h>
export module ModelVulkan;

import Camera;
import ErrorHandling;
import GraphicsPipelineVulkan;
import Model;
import SettingsRenderer;

namespace gg
{
	export struct ModelVulkan : public Model
	{
	public:
		ModelVulkan(std::string const& modelRelativePath, std::shared_ptr<ShaderProgram>, glm::vec3& position, VkDevice);
		~ModelVulkan() noexcept;

		std::expected<void, Error> Initialize(VkDescriptorPool);
		std::expected<void, Error> CreatePipeline();
		void                       Draw(VkCommandBuffer, uint32_t frame, Camera const&, double currentTimeSec);

	private:
		std::expected<void, Error> CreateVertexBuffer();
		std::expected<void, Error> LoadTextures();
		std::expected<void, Error> CreateDescriptorSetLayout();
		std::expected<void, Error> CreateUniformBuffers();
		std::expected<void, Error> CreateDescriptorSets(VkDescriptorPool);
		void                       DestroyPipeline();
		void                       UpdateUniformBuffers(uint32_t frame, Camera const&, double currentTimeSec);

		VkDevice       device;
		VkBuffer       vertexBuffer{};
		VkDeviceMemory vertexBufferMemory{};

		std::array<VkBuffer, maxFramesInFlight>       uniformBuffersMVP{};
		std::array<VkDeviceMemory, maxFramesInFlight> uniformBuffersMemoryMVP{};

		std::array<VkBuffer, maxFramesInFlight>       uniformBuffersDirLight{};
		std::array<VkDeviceMemory, maxFramesInFlight> uniformBuffersMemoryDirLight{};

		VkDescriptorSetLayout                          descriptorSetLayout{};
		std::array<VkDescriptorSet, maxFramesInFlight> descriptorSets{};

		GraphicsPipeline graphicsPipeline{};
	};
} // namespace gg
