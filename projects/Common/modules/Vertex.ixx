module;
#include <DirectXMath.h>
export module Vertex;

using DirectX::XMVECTOR;
using DirectX::XMFLOAT2;

namespace gg
{
	export struct Vertex
	{
		XMVECTOR position{};
		XMFLOAT2 textureCoords0{};

		Vertex() = default;

		Vertex(float x, float y, float z, float w)
			: position{ x, y, z, w }
		{
		}

		Vertex(float x, float y, float z, float w, float u, float v)
			: position{ x, y, z, w }
			, textureCoords0{ u, v }
		{
		}
	};

} // namespace gg
