module;
#include <array>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <memory>
#include <string>
#include <vulkan/vulkan.h>
export module ModelVulkan;

import Model;
import SettingsRenderer;

namespace gg
{
	export struct ModelVulkan : public Model
	{
	public:
		ModelVulkan(std::string const& modelRelativePath, std::shared_ptr<ShaderProgram>, glm::vec3& position, VkDevice);
		~ModelVulkan() noexcept;

	private:
		VkDevice device;
		VkBuffer VB{};
		VkDeviceMemory vertexBufferMemory{};

		std::array<VkBuffer, maxFramesInFlight> uniformBuffersMVP;
		std::array<VkDeviceMemory, maxFramesInFlight> uniformBuffersMemoryMVP;

		std::array<VkBuffer, maxFramesInFlight> uniformBuffersDirLight;
		std::array<VkDeviceMemory, maxFramesInFlight> uniformBuffersMemoryDirLight;

		std::array<VkDescriptorSet, maxFramesInFlight> descriptorSets;

		VkPipeline graphicsPipeline{};
		VkPipelineLayout pipelineLayout{};

		friend class RendererVulkan;
	};
} //namespace gg
