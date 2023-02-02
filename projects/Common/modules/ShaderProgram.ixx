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
		virtual ~ShaderProgram() = default;
	};
} // namespace gg
