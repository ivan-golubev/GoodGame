module;
#include <string>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
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

	struct ModelViewProjectionCB
	{
		glm::mat4x4 MVP;
		glm::vec4 ViewPosition;
	};

} // namespace gg
