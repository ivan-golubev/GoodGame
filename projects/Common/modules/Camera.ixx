module;
#include <cstdint>
#include <chrono>
#include <memory>
#include <DirectXMath.h>
export module Camera;

import Input;

using DirectX::XMMATRIX;
using DirectX::XMVECTOR;

export namespace gg
{
	class Camera
	{
	public:
		Camera(std::shared_ptr<InputManager>);
		void UpdateCamera(std::chrono::nanoseconds deltaTime);
		void UpdateProjectionMatrix(float windowAspectRatio);
		XMMATRIX const& GetViewMatrix() const;
		XMMATRIX const& GetProjectionMatrix() const;
		XMVECTOR const& GetCameraPosition() const;
	private:
		XMMATRIX projectionMatrix{};
		XMMATRIX viewMatrix{};

		XMVECTOR cameraPosition{ 0.f, 0.f, -4.f, 1.f };
		XMVECTOR focusPoint{ 0.f, 0.f, 0.f, 1.f };

		std::shared_ptr<InputManager> inputManager;
	};

} // namespace gg