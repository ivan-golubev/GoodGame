module;
#include <string>
#include <vulkan/vulkan.h>
export module ShaderProgram;

export namespace gg
{
	constexpr char const* entryPointVertexShader{ "vs_main" };
	constexpr char const* entryPointFragmentShader{ "ps_main" }; // TODO: this should be fs_main

	class ShaderProgram
	{
	public:
		ShaderProgram() = default;
		ShaderProgram(std::string const& vertexShaderAbsPath, std::string const& fragmentShaderAbsPath);

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

		VkShaderModule vertexShader{ nullptr };
		VkShaderModule fragmentShader{ nullptr };
		VkDevice device;
	};
} // namespace gg
