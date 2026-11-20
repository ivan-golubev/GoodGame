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
		/* Only RendererVulkan can make a PassKey, so only it can create a ShaderProgramVulkan,
		   while std::make_unique can still call the public constructor. */
		class PassKey
		{
			friend class RendererVulkan;
			PassKey() = default;
		};
		ShaderProgramVulkan(PassKey, VkDevice);

		ShaderProgramVulkan(ShaderProgramVulkan&&) noexcept;
		ShaderProgramVulkan& operator=(ShaderProgramVulkan&&) noexcept;

		/* Prohibit copying to make sure the underlying shader is destroyed exactly once */
		ShaderProgramVulkan(ShaderProgramVulkan const&)            = delete;
		ShaderProgramVulkan& operator=(ShaderProgramVulkan const&) = delete;

		~ShaderProgramVulkan() noexcept;

		VkShaderModule GetVertexShader() const;
		VkShaderModule GetFragmentShader() const;

	private:
		/* Only RendererVulkan initializes the shaders */
		friend class RendererVulkan;
		std::expected<void, Error> Initialize(std::string const& vertexShaderRelativePath, std::string const& fragmentShaderRelativePath);

		std::string vertexShaderBlob;
		std::string fragmentShaderBlob;

		VkDevice       device{};
		VkShaderModule vertexShader{ nullptr };
		VkShaderModule fragmentShader{ nullptr };
	};
}
