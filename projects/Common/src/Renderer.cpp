module;
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat3x3.hpp>
#include <glm/matrix.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <memory>
#include <numbers>
module Renderer;

import Camera;
import GlobalSettings;
import SettingsRenderer;
import ShaderProgram;

namespace gg
{
	glm::mat4x4 CalculateMVP(glm::mat4x4 modelTranslation, double currentTimeSec, Camera const& camera)
	{
		/* Rotate the model */
		float rotation = static_cast<float>(cubeRotationSpeed * std::numbers::pi_v<double> *currentTimeSec);
		glm::mat4x4 const modelMatrix{ glm::rotate(modelTranslation, rotation, glm::vec3(1.f, 0.f, 1.f)) };
		return camera.GetProjectionMatrix() * camera.GetViewMatrix() * modelMatrix;
	}

	SkyboxCameraCB CalculateSkyboxCamera(Camera const& camera)
	{
		return SkyboxCameraCB{
			/* screen position -> view-space point, used to rebuild the view ray per pixel */
			.InvProjection = glm::inverse(camera.GetProjectionMatrix()),
			/* view-space ray -> world space; rotation only, so the sky ignores the camera position.
			   The inverse of a pure rotation is its transpose. */
			.InvViewRotation = glm::mat4x4{ glm::transpose(glm::mat3x3{ camera.GetViewMatrix() }) },
		};
	}
} // namespace gg