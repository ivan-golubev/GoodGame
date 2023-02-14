module;
#include <chrono>
#include <cstdint>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <memory>
export module Camera;

import Input;

export namespace gg
{
	class Camera
	{
	public:
		Camera(std::shared_ptr<InputManager>);
		void UpdateCamera(std::chrono::nanoseconds deltaTime);
		void UpdateProjectionMatrix(uint32_t width, uint32_t height);
		glm::mat4x4 const& GetViewMatrix() const;
		glm::mat4x4 const& GetProjectionMatrix() const;
		glm::vec4 GetCameraPosition() const;
	private:
		glm::mat4x4 projectionMatrix{};
		glm::mat4x4 viewMatrix{};

		glm::vec3 cameraPosition{ 0.f, 0.f, -4.f };
		glm::vec3 focusPoint{ 0.f, 0.f, 0.f };

		std::shared_ptr<InputManager> inputManager;
	};

} // namespace gg