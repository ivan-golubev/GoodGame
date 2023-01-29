module;
#include <memory>
#include <string>
#include <utility>
#include <vulkan/vulkan.h>
module ShaderProgramVulkan;

import Application;
import ErrorHandling;
import RendererVulkan;

namespace
{
	using gg::AssetLoadException;

	VkShaderModule createShaderModule(VkDevice device, std::string const& shaderBlob)
	{
		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = shaderBlob.size();
		createInfo.pCode = reinterpret_cast<const uint32_t*>(shaderBlob.data());
		VkShaderModule shaderModule;
		if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
		{
			throw AssetLoadException("failed to create shader module!");
		}
		return shaderModule;
	}
}

namespace gg
{
	ShaderProgramVulkan::ShaderProgramVulkan(std::string const& vertexShaderRelativePath, std::string const& fragmentShaderRelativePath, VkDevice d)
		: ShaderProgram(vertexShaderRelativePath, fragmentShaderRelativePath)
		, device { d }
	{
		VkDevice device{ RendererVulkan::Get()->GetDevice() };
		vertexShader = createShaderModule(device, vertexShaderBlob);
		fragmentShader = createShaderModule(device, fragmentShaderBlob);

		vertexShaderBlob.clear();
		fragmentShaderBlob.clear();
	}

	ShaderProgramVulkan::~ShaderProgramVulkan()
	{
		if (vertexShader)
			vkDestroyShaderModule(device, vertexShader, nullptr);
		if (fragmentShader)
			vkDestroyShaderModule(device, fragmentShader, nullptr);
	}

	ShaderProgramVulkan::ShaderProgramVulkan(ShaderProgramVulkan&& other) noexcept
	{
		vertexShader = nullptr;
		fragmentShader = nullptr;
		std::exchange(vertexShader, other.vertexShader);
		std::exchange(fragmentShader, other.fragmentShader);
	}

	ShaderProgramVulkan& ShaderProgramVulkan::operator=(ShaderProgramVulkan&& other) noexcept
	{
		if (this != &other)
		{
			vertexShader = nullptr;
			fragmentShader = nullptr;
			std::exchange(vertexShader, other.vertexShader);
			std::exchange(fragmentShader, other.fragmentShader);
		}
		return *this;
	}

	VkShaderModule ShaderProgramVulkan::GetVertexShader() const { return vertexShader; }
	VkShaderModule ShaderProgramVulkan::GetFragmentShader() const { return fragmentShader; }

} // namespace gg
