module;
#include <cstdint>
#include <chrono>
#include <DirectXMath.h>
export module Camera;

using DirectX::XMMATRIX;
using DirectX::XMVECTOR;

export namespace gg {

	class Camera
	{
	public:
		void UpdateCamera(std::chrono::milliseconds deltaTime);
		void UpdateProjectionMatrix(float windowAspectRatio);
		XMMATRIX const & GetViewMatrix() const;
		XMMATRIX const & GetProjectionMatrix() const;
	private:
		XMMATRIX projectionMatrix{};
		XMMATRIX viewMatrix{};

		XMVECTOR cameraPosition{ 0.f, 0.f, -3.f, 1.f };
		XMVECTOR focusPoint{ 0.f, 0.f, 0.f, 1.f };
	};

} // namespace gg