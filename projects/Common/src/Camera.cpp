module;
#include <chrono>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec4.hpp>
#include <numbers>
module Camera;

import Input;
import Application;

namespace gg
{
	constexpr double cameraMoveSpeed{ 5.0 }; // in metres per second
	constexpr double cameraTurnSpeed{ std::numbers::pi_v<double> }; // in radians per second
	constexpr float fieldOfView{ 90.f };
	constexpr float nearPlane{ 0.1f };
	constexpr float farPlane{ 100.f };

	constexpr glm::vec3 forward{ 0.f, 0.f, 1.f };
	constexpr glm::vec3 up{ 0.f, 1.f, 0.f };
	constexpr glm::vec3 right{ 1.f, 0.f, 0.f };

	Camera::Camera(std::shared_ptr<InputManager> inputManager)
		: inputManager{ inputManager }
	{
	}

	void Camera::UpdateCamera(std::chrono::nanoseconds deltaTime)
	{
		using namespace std::chrono_literals;

		double const deltaTimeSeconds{ deltaTime / 1.0s };
		float const cameraMoveAmount = static_cast<float>(cameraMoveSpeed * deltaTimeSeconds);
		{
			glm::vec3 const moveFB{ forward * cameraMoveAmount };
			if (inputManager->IsKeyDown(InputAction::MoveCameraForward))
				cameraPosition += moveFB;
			if (inputManager->IsKeyDown(InputAction::MoveCameraBack))
				cameraPosition -= moveFB;
		}
		{
			glm::vec3 const moveLR{ right * cameraMoveAmount };
			if (inputManager->IsKeyDown(InputAction::MoveCameraRight))
			{
				focusPoint += moveLR;
				cameraPosition += moveLR;
			}
			if (inputManager->IsKeyDown(InputAction::MoveCameraLeft))
			{
				focusPoint -= moveLR;
				cameraPosition -= moveLR;
			}
		}
		{
			glm::vec3 const moveUD{ up * cameraMoveAmount };
			if (inputManager->IsKeyDown(InputAction::RaiseCamera))
			{
				cameraPosition += moveUD;
				focusPoint += moveUD;
			}
			if (inputManager->IsKeyDown(InputAction::LowerCamera))
			{
				cameraPosition -= moveUD;
				focusPoint -= moveUD;
			}
		}
		{
			float const cameraTurnAmount = static_cast<float>(cameraTurnSpeed * deltaTimeSeconds);
			glm::vec3 const moveLR{ right * cameraTurnAmount };
			if (inputManager->IsKeyDown(InputAction::TurnCameraRight))
				focusPoint += moveLR;
			if (inputManager->IsKeyDown(InputAction::TurnCameraLeft))
				focusPoint -= moveLR;

			glm::vec3 const moveUP{ up * cameraTurnAmount };
			if (inputManager->IsKeyDown(InputAction::LookCameraUp))
				focusPoint += moveUP;
			if (inputManager->IsKeyDown(InputAction::LookCameraDown))
				focusPoint -= moveUP;
		}
		viewMatrix = glm::lookAtLH(cameraPosition, focusPoint, up);
	}

	void Camera::UpdateProjectionMatrix(uint32_t width, uint32_t height)
	{

		projectionMatrix = glm::perspectiveFovLH(glm::radians(fieldOfView), static_cast<float>(width), static_cast<float>(height), nearPlane, farPlane);
	}

	glm::mat4x4 const& Camera::GetViewMatrix() const { return viewMatrix; }
	glm::mat4x4 const& Camera::GetProjectionMatrix() const { return projectionMatrix; }

	glm::vec4 Camera::GetPositionVec4() const { return glm::vec4(cameraPosition, 1.0f); }

	glm::vec3* Camera::GetPosition() { return &cameraPosition; }
	glm::vec3* Camera::GetFocusPoint() { return &focusPoint; }

} // namespace gg