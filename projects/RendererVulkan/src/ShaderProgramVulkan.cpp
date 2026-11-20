module;
#include <expected>
#include <filesystem>
#include <format>
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
	std::expected<VkShaderModule, gg::Error> CreateShaderModule(
		VkDevice           device,
		std::string const& shaderBlob,
		std::string const& shaderPath
	)
	{
		VkShaderModuleCreateInfo createInfo{
			.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			.codeSize = shaderBlob.size(),
			.pCode    = reinterpret_cast<uint32_t const*>(shaderBlob.data()),
		};
		VkShaderModule shaderModule{};
		VkResult const result{ vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) };
		if (result != VK_SUCCESS)
		{
			auto const message = std::format(
				"Failed to create Vulkan shader module from '{}': VkResult {}",
				shaderPath,
				static_cast<int>(result)
			);
			return std::unexpected(gg::Error{
				.code    = gg::ErrorCode::VulkanShaderModuleCreationFailed,
				.message = message,
			});
		}
		return shaderModule;
	}

	std::expected<std::string, gg::Error> ReadFile(std::string const& filename)
	{
		std::ifstream file(filename, std::ios::ate | std::ios::binary);

		if (!file.is_open())
			return std::unexpected(gg::Error{
				.code    = gg::ErrorCode::VulkanShaderFileReadFailed,
				.message = std::format("Failed to open shader file '{}' for reading", filename),
			});

		std::streampos const fileSize{ file.tellg() };
		if (fileSize <= 0)
			return std::unexpected(gg::Error{
				.code    = gg::ErrorCode::VulkanShaderFileEmpty,
				.message = std::format("Shader file '{}' is empty or its size could not be read", filename),
			});
		std::string buffer{};
		buffer.resize(static_cast<size_t>(fileSize));
		file.seekg(0);
		file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
		if (!file)
			return std::unexpected(gg::Error{
				.code    = gg::ErrorCode::VulkanShaderFileReadFailed,
				.message = std::format("Failed while reading shader file '{}'", filename),
			});
		return buffer;
	}
}

namespace gg
{
	ShaderProgramVulkan::ShaderProgramVulkan(VkDevice d)
		: device{ d }
	{
	}

	std::expected<void, Error> ShaderProgramVulkan::Initialize(
		std::string const& vertexShaderRelativePath,
		std::string const& fragmentShaderRelativePath
	)
	{
		std::string const vertexShaderPath{ std::filesystem::absolute(vertexShaderRelativePath).generic_string() };
		std::string const fragmentShaderPath{ std::filesystem::absolute(fragmentShaderRelativePath).generic_string() };
		auto              vertexBlobResult{ ReadFile(vertexShaderPath) };
		if (!vertexBlobResult)
			return std::unexpected(vertexBlobResult.error());
		vertexShaderBlob = std::move(*vertexBlobResult);
		auto fragmentBlobResult{ ReadFile(fragmentShaderPath) };
		if (!fragmentBlobResult)
			return std::unexpected(fragmentBlobResult.error());
		fragmentShaderBlob = std::move(*fragmentBlobResult);

		auto vertexModuleResult{ CreateShaderModule(device, vertexShaderBlob, vertexShaderPath) };
		if (!vertexModuleResult)
			return std::unexpected(vertexModuleResult.error());
		vertexShader = *vertexModuleResult;
		auto fragmentModuleResult{ CreateShaderModule(device, fragmentShaderBlob, fragmentShaderPath) };
		if (!fragmentModuleResult)
			return std::unexpected(fragmentModuleResult.error());
		fragmentShader = *fragmentModuleResult;

		vertexShaderBlob.clear();
		fragmentShaderBlob.clear();
		return {};
	}

	ShaderProgramVulkan::~ShaderProgramVulkan() noexcept
	{
		if (device && vertexShader)
			vkDestroyShaderModule(device, vertexShader, nullptr);
		if (device && fragmentShader)
			vkDestroyShaderModule(device, fragmentShader, nullptr);
	}

	ShaderProgramVulkan::ShaderProgramVulkan(ShaderProgramVulkan&& other) noexcept
		: vertexShaderBlob{ std::move(other.vertexShaderBlob) }
		, fragmentShaderBlob{ std::move(other.fragmentShaderBlob) }
		, device{ std::exchange(other.device, VK_NULL_HANDLE) }
		, vertexShader{ std::exchange(other.vertexShader, VK_NULL_HANDLE) }
		, fragmentShader{ std::exchange(other.fragmentShader, VK_NULL_HANDLE) }
	{
	}

	ShaderProgramVulkan& ShaderProgramVulkan::operator=(ShaderProgramVulkan&& other) noexcept
	{
		if (this != &other)
		{
			if (device && vertexShader)
				vkDestroyShaderModule(device, vertexShader, nullptr);
			if (device && fragmentShader)
				vkDestroyShaderModule(device, fragmentShader, nullptr);

			vertexShaderBlob   = std::move(other.vertexShaderBlob);
			fragmentShaderBlob = std::move(other.fragmentShaderBlob);
			device             = std::exchange(other.device, VK_NULL_HANDLE);
			vertexShader       = std::exchange(other.vertexShader, VK_NULL_HANDLE);
			fragmentShader     = std::exchange(other.fragmentShader, VK_NULL_HANDLE);
		}
		return *this;
	}

	VkShaderModule ShaderProgramVulkan::GetVertexShader() const { return vertexShader; }
	VkShaderModule ShaderProgramVulkan::GetFragmentShader() const { return fragmentShader; }

} // namespace gg
