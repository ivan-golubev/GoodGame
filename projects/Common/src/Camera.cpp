module;
#include <DirectXMath.h>
#include <chrono>
#include <numbers>
module Camera;

import Input;
import Application;

using DirectX::XMVECTOR;
using DirectX::XMMATRIX;

namespace gg
{
	constexpr double cameraMoveSpeed{ 5.0 }; // in metres per second
	constexpr double cameraTurnSpeed{ std::numbers::pi_v<double> }; // in radians per second
	constexpr float fieldOfView{ 90.f };
	constexpr float near{ 0.1f };
	constexpr float far{ 100.f };

	constexpr XMVECTOR forward{ 0.f, 0.f, 1.f };
	constexpr XMVECTOR up{ 0.f, 1.f, 0.f, 0.f };
	constexpr XMVECTOR right{ 1.f, 0.f, 0.f };

	Camera::Camera(std::shared_ptr<InputManager> inputManager)
		: inputManager{ inputManager }
	{
	}

	void Camera::UpdateCamera(std::chrono::nanoseconds deltaTime)
	{
		using namespace DirectX;
		using namespace std::chrono_literals;

		double const deltaTimeSeconds{ deltaTime / 1.0s };
		float const cameraMoveAmount = static_cast<float>(cameraMoveSpeed * deltaTimeSeconds);
		{
			XMVECTOR const moveFB{ XMVectorScale(forward, cameraMoveAmount) };
			if (inputManager->IsKeyDown(InputAction::MoveCameraForward))
				cameraPosition = XMVectorAdd(cameraPosition, moveFB);
			if (inputManager->IsKeyDown(InputAction::MoveCameraBack))
				cameraPosition = XMVectorSubtract(cameraPosition, moveFB);
		}
		{
			XMVECTOR const moveLR{ XMVectorScale(right, cameraMoveAmount) };
			if (inputManager->IsKeyDown(InputAction::MoveCameraRight))
				focusPoint = XMVectorAdd(focusPoint, moveLR);
			if (inputManager->IsKeyDown(InputAction::MoveCameraLeft))
				focusPoint = XMVectorSubtract(focusPoint, moveLR);
		}
		{
			XMVECTOR const moveUD{ XMVectorScale(up, cameraMoveAmount) };
			if (inputManager->IsKeyDown(InputAction::RaiseCamera))
			{
				cameraPosition = XMVectorAdd(cameraPosition, moveUD);
				focusPoint = XMVectorAdd(focusPoint, moveUD);
			}
			if (inputManager->IsKeyDown(InputAction::LowerCamera))
			{
				cameraPosition = XMVectorSubtract(cameraPosition, moveUD);
				focusPoint = XMVectorSubtract(focusPoint, moveUD);
			}
		}
		{
			float const CAM_TURN_AMOUNT = static_cast<float>(cameraTurnSpeed * deltaTimeSeconds);
			XMVECTOR const moveLR{ XMVectorScale(right, CAM_TURN_AMOUNT) };
			if (inputManager->IsKeyDown(InputAction::TurnCameraRight))
				focusPoint = XMVectorAdd(focusPoint, moveLR);
			if (inputManager->IsKeyDown(InputAction::TurnCameraLeft))
				focusPoint = XMVectorSubtract(focusPoint, moveLR);

			XMVECTOR const moveUP{ XMVectorScale(up, CAM_TURN_AMOUNT) };
			if (inputManager->IsKeyDown(InputAction::LookCameraUp))
				focusPoint = XMVectorAdd(focusPoint, moveUP);
			if (inputManager->IsKeyDown(InputAction::LookCameraDown))
				focusPoint = XMVectorSubtract(focusPoint, moveUP);
		}
		viewMatrix = XMMatrixLookAtLH(cameraPosition, focusPoint, up);
	}

	void Camera::UpdateProjectionMatrix(float windowAspectRatio)
	{
		using namespace DirectX;
		projectionMatrix = XMMatrixPerspectiveFovLH(XMConvertToRadians(fieldOfView), windowAspectRatio, near, far);
	}

	XMMATRIX const& Camera::GetViewMatrix() const { return viewMatrix; }
	XMMATRIX const& Camera::GetProjectionMatrix() const { return projectionMatrix; }

} // namespace gg