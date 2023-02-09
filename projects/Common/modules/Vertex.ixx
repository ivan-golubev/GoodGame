module;
#include <DirectXMath.h>
export module Vertex;

using DirectX::XMVECTOR;
using DirectX::XMFLOAT2;
using DirectX::XMFLOAT3;

namespace gg
{
	export struct Vertex
	{
		XMVECTOR position{};
		XMFLOAT2 textureCoords0{};
		XMFLOAT3 normals{};

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

		Vertex(float x, float y, float z, float w, float u, float v, float nx, float ny, float nz)
			: position{ x, y, z, w }
			, textureCoords0{ u, v }
			, normals{ nx, ny, nz }
		{
		}
	};

} // namespace gg
