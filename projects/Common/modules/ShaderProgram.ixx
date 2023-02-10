module;
#include <string>
#include <DirectXMath.h>
export module ShaderProgram;

using DirectX::XMMATRIX;
using DirectX::XMVECTOR;

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
		XMMATRIX MVP;
		XMMATRIX M;
		XMVECTOR ViewPosition;
	};

} // namespace gg
