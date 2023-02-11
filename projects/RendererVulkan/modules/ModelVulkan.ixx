module;
#include <memory>
#include <string>
#include <array>
#include <vulkan/vulkan.h>
#include <DirectXMath.h>
export module ModelVulkan;

import Model;
import Renderer;

using DirectX::XMVECTOR;

namespace gg
{
	export struct ModelVulkan : public Model
	{
	public:
		ModelVulkan(std::string const& modelRelativePath, std::shared_ptr<ShaderProgram>, XMVECTOR& position, VkDevice);
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
