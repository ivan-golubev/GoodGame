module;
#include <DirectXMath.h>
export module Lighting;

using DirectX::XMVECTOR;
using DirectX::XMFLOAT3;

export namespace gg
{
	struct DirectionalLight
	{
		XMVECTOR diffuseColor;
		XMFLOAT3 lightDirection;

		constexpr DirectionalLight(float r, float g, float b, float a, float x, float y, float z)
			: diffuseColor{ r, g, b, a }
			, lightDirection{ x, y, z }
		{
		}
	};

} // namespace gg
