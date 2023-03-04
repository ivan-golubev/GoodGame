module;
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <string>
#include <vector>
#include <unordered_map>
export module ShaderProgram;

export namespace gg
{
	std::string const entryPointVertexShader{ "vs_main" };
	std::string const entryPointFragmentShader{ "ps_main" };

	enum class Semantic
	{
		Unknown,
		Position,
		Normal,
		TextureCoordinates
	};

	struct InputAttribute
	{
		Semantic semantic;
		uint32_t componentCount;
	};

	std::unordered_map<std::string, Semantic> const semanticNameToSemantic{
		{ "POSITION", Semantic::Position },
		{ "NORMAL", Semantic::Normal },
		{ "TEXCOORD", Semantic::TextureCoordinates }
	};

	class ShaderProgram
	{
	public:
		virtual ~ShaderProgram() = default;

		std::unordered_map<Semantic, InputAttribute> inputAttributes;
	};

	struct ModelViewProjectionCB
	{
		glm::mat4x4 MVP;
		glm::vec4 ViewPosition;
	};

} // namespace gg
