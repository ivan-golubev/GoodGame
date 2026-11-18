module;
#include <algorithm>
#include <chrono>
#include <cmath>
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
	constexpr double cameraTurnSpeed{ std::numbers::pi_v<double> }; // in radians per second, arrow keys
	constexpr float mouseSensitivity{ 0.0022f }; // in radians per pixel of mouse motion
	constexpr float maxPitch{ std::numbers::pi_v<float> * 89.f / 180.f }; // stop short of straight up/down, where lookAt degenerates
	constexpr float fieldOfView{ 90.f };
	constexpr float nearPlane{ 0.1f };
	constexpr float farPlane{ 100.f };

	constexpr glm::vec3 up{ 0.f, 1.f, 0.f };

	Camera::Camera(std::shared_ptr<InputManager> inputManager)
		: inputManager{ inputManager }
	{
	}

	void Camera::UpdateCamera(std::chrono::nanoseconds deltaTime)
	{
		using namespace std::chrono_literals;

		double const deltaTimeSeconds{ deltaTime / 1.0s };

		{	/* look: mouse motion (not scaled by frame time) plus arrow keys (scaled by frame time) */
			MouseDelta const mouse{ inputManager->ConsumeMouseDelta() };
			yaw += mouse.x * mouseSensitivity;
			pitch -= mouse.y * mouseSensitivity; // screen y grows downwards

			float const cameraTurnAmount = static_cast<float>(cameraTurnSpeed * deltaTimeSeconds);
			if (inputManager->IsKeyDown(InputAction::TurnCameraRight))
				yaw += cameraTurnAmount;
			if (inputManager->IsKeyDown(InputAction::TurnCameraLeft))
				yaw -= cameraTurnAmount;
			if (inputManager->IsKeyDown(InputAction::LookCameraUp))
				pitch += cameraTurnAmount;
			if (inputManager->IsKeyDown(InputAction::LookCameraDown))
				pitch -= cameraTurnAmount;

			yaw = std::remainder(yaw, 2.f * std::numbers::pi_v<float>); // keep in [-pi, pi]
			pitch = std::clamp(pitch, -maxPitch, maxPitch);
		}

		/* left-handed: +X right, +Y up, +Z forward */
		float const sinYaw{ std::sin(yaw) };
		float const cosYaw{ std::cos(yaw) };
		float const cosPitch{ std::cos(pitch) };
		glm::vec3 const lookDirection{ sinYaw * cosPitch, std::sin(pitch), cosYaw * cosPitch };

		{	/* move: forward/back and strafe stay on the horizontal plane; Q/E move along world up */
			glm::vec3 const moveForward{ sinYaw, 0.f, cosYaw };
			glm::vec3 const moveRight{ cosYaw, 0.f, -sinYaw };

			float const cameraMoveAmount = static_cast<float>(cameraMoveSpeed * deltaTimeSeconds);
			if (inputManager->IsKeyDown(InputAction::MoveCameraForward))
				cameraPosition += moveForward * cameraMoveAmount;
			if (inputManager->IsKeyDown(InputAction::MoveCameraBack))
				cameraPosition -= moveForward * cameraMoveAmount;
			if (inputManager->IsKeyDown(InputAction::MoveCameraRight))
				cameraPosition += moveRight * cameraMoveAmount;
			if (inputManager->IsKeyDown(InputAction::MoveCameraLeft))
				cameraPosition -= moveRight * cameraMoveAmount;
			if (inputManager->IsKeyDown(InputAction::RaiseCamera))
				cameraPosition += up * cameraMoveAmount;
			if (inputManager->IsKeyDown(InputAction::LowerCamera))
				cameraPosition -= up * cameraMoveAmount;
		}

		viewMatrix = glm::lookAtLH(cameraPosition, cameraPosition + lookDirection, up);
	}

	void Camera::UpdateProjectionMatrix(uint32_t width, uint32_t height)
	{

		/* Depth maps to 0 (near) .. 1 (far), the Vulkan/D3D12 convention: GLM_FORCE_DEPTH_ZERO_TO_ONE is set in props/GLM.props. */
		projectionMatrix = glm::perspectiveFovLH(glm::radians(fieldOfView), static_cast<float>(width), static_cast<float>(height), nearPlane, farPlane);
	}

	glm::mat4x4 const& Camera::GetViewMatrix() const { return viewMatrix; }
	glm::mat4x4 const& Camera::GetProjectionMatrix() const { return projectionMatrix; }

	glm::vec4 Camera::GetPositionVec4() const { return glm::vec4(cameraPosition, 1.0f); }

	glm::vec3* Camera::GetPosition() { return &cameraPosition; }
	float* Camera::GetYaw() { return &yaw; }
	float* Camera::GetPitch() { return &pitch; }

} // namespace gg