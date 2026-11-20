module;
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <string>
#include <string_view>
export module ShaderProgram;

export namespace gg
{
	constexpr std::string_view entryPointVertexShader{ "VSMain" };
	constexpr std::string_view entryPointFragmentShader{ "PSMain" };

	class ShaderProgram
	{
	public:
		virtual ~ShaderProgram() noexcept = default;
	};

	struct ModelViewProjection
	{
		glm::mat4x4 MVP;
		glm::mat4x4 model; // model to world space, for world-space normals and positions
		glm::vec4   viewPosition;
	};

	struct SkyboxCamera
	{
		glm::mat4x4 invProjection;
		glm::mat4x4 invViewRotation;
	};

} // namespace gg
