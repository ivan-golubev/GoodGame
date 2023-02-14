module;
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vulkan/vulkan.h>
module ShaderProgramVulkan;

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

	std::string readFile(std::string const& filename)
	{
		std::ifstream file(filename, std::ios::ate | std::ios::binary);

		if (!file.is_open())
		{
			throw AssetLoadException(std::format("failed to open file: {}", filename));
		}

		size_t fileSize = static_cast<size_t>(file.tellg());
		std::string buffer{};
		buffer.resize(fileSize);
		file.seekg(0);
		file.read(buffer.data(), fileSize);
		file.close();
		return buffer;
	}
}

namespace gg
{
	ShaderProgramVulkan::ShaderProgramVulkan(std::string const& vertexShaderRelativePath, std::string const& fragmentShaderRelativePath, VkDevice d)
		: vertexShaderBlob{ readFile(std::filesystem::absolute(vertexShaderRelativePath).generic_string()) }
		, fragmentShaderBlob{ readFile(std::filesystem::absolute(fragmentShaderRelativePath).generic_string()) }
		, device{ d }
	{
		BreakIfFalse(vertexShaderBlob.size() != 0);
		BreakIfFalse(fragmentShaderBlob.size() != 0);

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
