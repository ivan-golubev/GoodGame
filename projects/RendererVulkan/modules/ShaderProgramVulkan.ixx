module;
#include <string>
#include <vulkan/vulkan.h>
export module ShaderProgramVulkan;

import ShaderProgram;

export namespace gg
{
	class ShaderProgramVulkan : public ShaderProgram
	{
	public:
		ShaderProgramVulkan(ShaderProgramVulkan&&) noexcept;
		ShaderProgramVulkan& operator=(ShaderProgramVulkan&&) noexcept;

		/* Prohibit copying to make sure the underlying shader is destroyed exactly once */
		ShaderProgramVulkan(ShaderProgramVulkan const&) = delete;
		ShaderProgramVulkan& operator=(ShaderProgramVulkan const&) = delete;

		~ShaderProgramVulkan();

		VkShaderModule GetVertexShader() const;
		VkShaderModule GetFragmentShader() const;

	private:
		/* Only Vulkan Renderer can create ShaderProgramVulkan */
		friend class RendererVulkan;
		ShaderProgramVulkan() = default;
		ShaderProgramVulkan(std::string const& vertexShaderRelativePath, std::string const& fragmentShaderRelativePath, VkDevice);

		VkDevice device;
		VkShaderModule vertexShader{ nullptr };
		VkShaderModule fragmentShader{ nullptr };
	};
}