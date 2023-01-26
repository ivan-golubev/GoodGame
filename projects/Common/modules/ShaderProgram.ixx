module;
#include <string>
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

		virtual ~ShaderProgram() = default;

	protected:
		std::string vertexShaderBlob;
		std::string fragmentShaderBlob;
	};
} // namespace gg
