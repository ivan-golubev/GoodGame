module;
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <string>
export module ShaderProgram;

export namespace gg
{
	std::string const entryPointVertexShader{ "VSMain" };
	std::string const entryPointFragmentShader{ "PSMain" };

	class ShaderProgram
	{
	public:
		virtual ~ShaderProgram() noexcept = default;
	};

	struct ModelViewProjection
	{
		glm::mat4x4 MVP;
		glm::vec4   viewPosition;
	};

	struct SkyboxCamera
	{
		glm::mat4x4 invProjection;
		glm::mat4x4 invViewRotation;
	};

} // namespace gg
