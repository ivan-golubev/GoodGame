module;
#include <memory>
#include <string>
#include <vulkan/vulkan.h>
#include <DirectXMath.h>
export module ModelVulkan;

import Model;

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

		VkPipeline graphicsPipeline{};

		friend class RendererVulkan;
	};
} //namespace gg
