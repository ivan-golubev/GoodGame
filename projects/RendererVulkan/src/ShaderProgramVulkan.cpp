module;
#include <filesystem>
#include <fstream>
#include <memory>
#include <stddef.h>
#include <string>
#include <utility>
#include <vector>
#include <vulkan/vulkan.h>
#include <spirv_glsl.hpp>
module ShaderProgramVulkan;

import ShaderProgram;
import ErrorHandling;
import ErrorHandlingVulkan;
import RendererVulkan;

namespace
{
	using gg::AssetLoadException;

	VkShaderModule createShaderModule(VkDevice device, std::vector<uint32_t> const& shaderBlob)
	{
		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = shaderBlob.size() * sizeof(uint32_t);
		createInfo.pCode = shaderBlob.data();
		VkShaderModule shaderModule;
		if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
		{
			throw AssetLoadException("failed to create shader module!");
		}
		return shaderModule;
	}

	std::vector<uint32_t> readFile(std::string const& filename)
	{
		std::ifstream file(filename, std::ios::ate | std::ios::binary);

		if (!file.is_open())
		{
			throw AssetLoadException(std::format("failed to open file: {}", filename));
		}

		size_t fileSize = static_cast<size_t>(file.tellg());
		std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

		file.seekg(0);
		file.read((char*)buffer.data(), fileSize);
		file.close();
		return buffer;
	}

	VkFormat ToVkFormat(SPIRV_CROSS_NAMESPACE::SPIRType const& spirvType)
	{
		using namespace SPIRV_CROSS_NAMESPACE;

		uint32_t const componentCount = spirvType.vecsize * spirvType.columns;
		switch (componentCount)
		{
		case 1:
			switch (spirvType.basetype)
			{
			case SPIRType::BaseType::Float: return VK_FORMAT_R32_SFLOAT;
			}
		case 2:
			switch (spirvType.basetype)
			{
			case SPIRType::BaseType::Float: return VK_FORMAT_R32G32_SFLOAT;
			}
		case 3:
			switch (spirvType.basetype)
			{
			case SPIRType::BaseType::Float: return VK_FORMAT_R32G32B32_SFLOAT;
			}
		case 4:
			switch (spirvType.basetype)
			{
			case SPIRType::BaseType::Float: return VK_FORMAT_R32G32B32A32_SFLOAT;
			}
		default: throw gg::VulkanInitException("Unsupported vector size");
		}

		gg::BreakIfFalse(false); /* encountered an unexpected input type */
		return VK_FORMAT_UNDEFINED;
	}

	constexpr char const* inputVarPrefix{ "in.var" };
	constexpr size_t inputVarPrefixLength{ sizeof(inputVarPrefix) - 1 };
}

namespace gg
{
	ShaderProgramVulkan::ShaderProgramVulkan(std::string const& vertexShaderRelativePath, std::string const& fragmentShaderRelativePath, VkDevice d)
		: vertexShaderBlob{ readFile(std::filesystem::absolute(vertexShaderRelativePath).generic_string()) }
		, fragmentShaderBlob{ readFile(std::filesystem::absolute(fragmentShaderRelativePath).generic_string()) }
		, device{ d }
	{
		BreakIfFalse(!vertexShaderBlob.empty());
		BreakIfFalse(!fragmentShaderBlob.empty());

		VkDevice device{ RendererVulkan::Get()->GetDevice() };
		vertexShader = createShaderModule(device, vertexShaderBlob);
		fragmentShader = createShaderModule(device, fragmentShaderBlob);

		ReflectInputLayout();

		vertexShaderBlob.clear();
		fragmentShaderBlob.clear();
	}

	void ShaderProgramVulkan::ReflectInputLayout()
	{
		using namespace SPIRV_CROSS_NAMESPACE;
		/* reflect the shaders using SPIRV-Cross */
		CompilerGLSL compiler{ std::move(vertexShaderBlob) };
		ShaderResources resources{ compiler.get_shader_resources() };

		uint32_t offset{ 0 };
		for (Resource const& resource : resources.stage_inputs)
		{
			SPIRType const spirvType{ compiler.get_type(resource.base_type_id) };

			VkVertexInputAttributeDescription inputAttribute{};
			inputAttribute.binding = 0;
			inputAttribute.location = compiler.get_decoration(resource.id, spv::DecorationLocation);
			inputAttribute.format = ToVkFormat(spirvType);
			inputAttribute.offset = offset;

			uint32_t attributeSizeBytes{ spirvType.width / 8 * spirvType.vecsize * spirvType.columns };
			offset += attributeSizeBytes;

			vertexAttributeDesc.push_back(inputAttribute);

			{ /* save the semantic and component count of each attribute */
				std::string variableName = resource.name;
				variableName = variableName.substr(inputVarPrefixLength, variableName.size() - inputVarPrefixLength);
				Semantic semantic = semanticNameToSemantic.at(variableName);
				inputAttributes[semantic] = { semantic, spirvType.vecsize * spirvType.columns };
			}
		}

		vertexBindingDesc.binding = 0;
		vertexBindingDesc.stride = offset;
		vertexBindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		InitVertexInputInfo();
	}

	void ShaderProgramVulkan::InitVertexInputInfo()
	{
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexAttributeDesc.size());
		vertexInputInfo.pVertexBindingDescriptions = &vertexBindingDesc;
		vertexInputInfo.pVertexAttributeDescriptions = vertexAttributeDesc.data();
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

		vertexBindingDesc = other.vertexBindingDesc;
		std::exchange(vertexAttributeDesc, other.vertexAttributeDesc);
		InitVertexInputInfo();
	}

	ShaderProgramVulkan& ShaderProgramVulkan::operator=(ShaderProgramVulkan&& other) noexcept
	{
		if (this != &other)
		{
			vertexShader = nullptr;
			fragmentShader = nullptr;
			std::exchange(vertexShader, other.vertexShader);
			std::exchange(fragmentShader, other.fragmentShader);

			vertexBindingDesc = other.vertexBindingDesc;
			std::exchange(vertexAttributeDesc, other.vertexAttributeDesc);
			InitVertexInputInfo();
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
