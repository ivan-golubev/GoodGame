module;
#include <numbers>
#include <memory>
#include <DirectXMath.h>
module Renderer;

import GlobalSettings;
import Camera;

using namespace DirectX;

namespace gg
{
	XMMATRIX UpdateMVP(XMMATRIX modelTranslation, double currentTimeSec, Camera const& camera)
	{
		/* Rotate the model */
		float rotation = static_cast<float>(cubeRotationSpeed * std::numbers::pi_v<double> *currentTimeSec);
		XMMATRIX const modelMatrix{ XMMatrixMultiply(XMMatrixMultiply(XMMatrixRotationY(rotation), XMMatrixRotationZ(rotation)), modelTranslation) };
		XMMATRIX mvpMatrix{ XMMatrixMultiply(modelMatrix, camera.GetViewMatrix()) };
		return XMMatrixMultiply(mvpMatrix, camera.GetProjectionMatrix());
	}
} // namespace gg