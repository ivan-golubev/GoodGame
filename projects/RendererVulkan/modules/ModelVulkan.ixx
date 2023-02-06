module;
#include <memory>
#include <string>
#include <array>
#include <vulkan/vulkan.h>
#include <DirectXMath.h>
export module ModelVulkan;

import Model;
import GlobalSettings;

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

		std::array<VkBuffer, maxFramesInFlight> uniformBuffers;
		std::array<VkDeviceMemory, maxFramesInFlight> uniformBuffersMemory;

		std::array<VkDescriptorSet, maxFramesInFlight> descriptorSets;

		VkPipeline graphicsPipeline{};
		VkPipelineLayout pipelineLayout{};

		friend class RendererVulkan;
	};
} //namespace gg
