module;
#include <chrono>
#include <cstdint>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <memory>
export module Camera;

export import ObjectId;
import Input;

export namespace gg
{
	class Camera
	{
	public:
		Camera(ObjectId, std::shared_ptr<InputManager>);
		ObjectId           GetId() const { return id; }
		void               UpdateCamera(std::chrono::nanoseconds deltaTime);
		void               UpdateProjectionMatrix(uint32_t width, uint32_t height);
		glm::mat4x4 const& GetViewMatrix() const;
		glm::mat4x4 const& GetProjectionMatrix() const;
		glm::vec4          GetPositionVec4() const; /* returns a copy */

		/* direct accessors */
		glm::vec3* GetPosition();
		float*     GetYaw(); /* radians, 0 = looking along +Z, positive = turned right */
		float*     GetPitch(); /* radians, 0 = level, positive = looking up */
	private:
		ObjectId id{ noObjectId };

		glm::mat4x4 projectionMatrix{};
		glm::mat4x4 viewMatrix{};

		glm::vec3 cameraPosition{ 0.f, 0.f, -4.f };
		float     yaw{ 0.f };
		float     pitch{ 0.f };

		std::shared_ptr<InputManager> inputManager;
	};

} // namespace gg
