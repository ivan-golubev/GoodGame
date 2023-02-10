module;
#include <DirectXMath.h>
export module Lighting;

using DirectX::XMVECTOR;
using DirectX::XMFLOAT3;

export namespace gg
{
	struct DirectionalLight
	{
		XMFLOAT3 diffuseColor;
		XMFLOAT3 lightDirection;

		constexpr DirectionalLight(float r, float g, float b, float x, float y, float z)
			: diffuseColor{ r, g, b }
			, lightDirection{ x, y, z }
		{
		}
	};

} // namespace gg
