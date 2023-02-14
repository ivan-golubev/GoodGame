module;
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <memory>
#include <numbers>
module Renderer;

import Camera;
import GlobalSettings;
import SettingsRenderer;

namespace gg
{
	glm::mat4x4 CalculateMVP(glm::mat4x4 modelTranslation, double currentTimeSec, Camera const& camera)
	{
		/* Rotate the model */
		float rotation = static_cast<float>(cubeRotationSpeed * std::numbers::pi_v<double> *currentTimeSec);
		glm::mat4x4 const modelMatrix{ glm::rotate(modelTranslation, rotation, glm::vec3(1.f, 0.f, 1.f)) };
		return camera.GetProjectionMatrix() * camera.GetViewMatrix() * modelMatrix;
	}
} // namespace gg