module;
#include <string>
#include <vulkan/vulkan.h>
export module ShaderProgram;

export namespace gg
{
	constexpr char const* VS_ENTRY_POINT{ "vs_main" };
	constexpr char const* FS_ENTRY_POINT{ "ps_main" };

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
