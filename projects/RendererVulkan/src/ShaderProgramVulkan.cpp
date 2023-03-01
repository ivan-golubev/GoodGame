module;
#include <filesystem>
#include <fstream>
#include <memory>
#include <stddef.h>
#include <string>
#include <utility>
#include <vulkan/vulkan.h>
module ShaderProgramVulkan;

import ErrorHandling;
import RendererVulkan;
import Vertex;

namespace
{
	using gg::AssetLoadException;
	using gg::Vertex;

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

	// TODO: these input layout should be reflected from a shader, not hardcoded
	VkVertexInputBindingDescription GetVertexBindingDescription()
	{
		VkVertexInputBindingDescription bindingDescription{};
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(Vertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		return bindingDescription;
	}

	std::vector<VkVertexInputAttributeDescription> GetVertexAttributeDescriptions()
	{
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions(3);
		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(Vertex, position);

		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(Vertex, normal);

		attributeDescriptions[2].binding = 0;
		attributeDescriptions[2].location = 2;
		attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[2].offset = offsetof(Vertex, textureCoords0);

		return attributeDescriptions;
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

		{ /* init vertex attributes */
			vertexBindingDesc = GetVertexBindingDescription();
			vertexAttributeDesc = GetVertexAttributeDescriptions();

			vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
			vertexInputInfo.vertexBindingDescriptionCount = 1;
			vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexAttributeDesc.size());
			vertexInputInfo.pVertexBindingDescriptions = &vertexBindingDesc;
			vertexInputInfo.pVertexAttributeDescriptions = vertexAttributeDesc.data();
		}
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

	VkPipelineVertexInputStateCreateInfo& ShaderProgramVulkan::GetInputLayout()
	{
		return vertexInputInfo;
	}

} // namespace gg
