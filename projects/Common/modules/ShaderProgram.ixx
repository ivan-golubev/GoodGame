module;
#include <string>
#include <vulkan/vulkan.h>
export module ShaderProgram;

export namespace gg
{
	std::string const entryPointVertexShader{ "vs_main" };
	std::string const entryPointFragmentShader{ "ps_main" };

	class ShaderProgram
	{
	public:
		ShaderProgram() = default;
		ShaderProgram(std::string const& vertexShaderRelativePath, std::string const& fragmentShaderRelativePath);

		ShaderProgram(ShaderProgram&&) noexcept;
		ShaderProgram& operator=(ShaderProgram&&) noexcept;

		/* Prohibit copying to make sure the underlying shader is destroyed exactly once */
		ShaderProgram(ShaderProgram const&) = delete;
		ShaderProgram& operator=(ShaderProgram const&) = delete;

		~ShaderProgram();

		VkShaderModule GetVertexShader() const;
		VkShaderModule GetFragmentShader() const;

	private:
		std::string vertexShaderBlob;
		std::string fragmentShaderBlob;


		// TODO: get rid of Vulkan-specific stuff in the "Common" project
		VkShaderModule vertexShader{ nullptr };
		VkShaderModule fragmentShader{ nullptr };
		VkDevice device{ nullptr };
	};
} // namespace gg
