module;
#include <expected>
#include <string>
#include <vulkan/vulkan.h>
export module ShaderProgramVulkan;

export import ErrorHandling;
import ShaderProgram;

export namespace gg
{
	class ShaderProgramVulkan : public ShaderProgram
	{
	public:
		explicit ShaderProgramVulkan(VkDevice);

		ShaderProgramVulkan(ShaderProgramVulkan&&) noexcept;
		ShaderProgramVulkan& operator=(ShaderProgramVulkan&&) noexcept;

		/* Loads the compiled vertex and fragment shaders from disk and creates their shader modules */
		std::expected<void, Error> Initialize(std::string const& vertexShaderRelativePath, std::string const& fragmentShaderRelativePath);

		/* Prohibit copying to make sure the underlying shader is destroyed exactly once */
		ShaderProgramVulkan(ShaderProgramVulkan const&)            = delete;
		ShaderProgramVulkan& operator=(ShaderProgramVulkan const&) = delete;

		~ShaderProgramVulkan() noexcept;

		VkShaderModule GetVertexShader() const;
		VkShaderModule GetFragmentShader() const;

	private:
		std::string vertexShaderBlob;
		std::string fragmentShaderBlob;

		VkDevice       device{};
		VkShaderModule vertexShader{ nullptr };
		VkShaderModule fragmentShader{ nullptr };
	};
}
