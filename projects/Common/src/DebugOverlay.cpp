module;
#include <glm/ext/vector_float3.hpp>
#include <imgui.h>
#include <memory>
module DebugOverlay;

import SettingsRenderer;
import Camera;
import Lighting;
import Renderer;

namespace
{
	void ImguiFloat3(char const* label, void* data)
	{
		ImGui::InputScalarN(label, ImGuiDataType_Float, data, 3, nullptr, nullptr, "%.3f", ImGuiInputTextFlags_CharsScientific);
	}

	constexpr ImVec4 rendererNameColor[2]{
		ImVec4(1.f, 0.f, 0.f, 1.f),
		ImVec4(0.f, 1.f, 0.f, 1.f)
	};
} // namespace

namespace gg
{
	void RenderDebugOverlay(Renderer& renderer, Scene& scene)
	{
		ImGui::NewFrame();
		ImGui::Begin("Renderer settings");

		Camera& camera{ scene.camera };

		RendererType const rendererType{ renderer.GetType() };
		ImGui::TextColored(rendererNameColor[rendererType], "Renderer: %s", ToString(rendererType).c_str());

		if (ImGui::CollapsingHeader("Light settings", ImGuiTreeNodeFlags_CollapsingHeader | ImGuiTreeNodeFlags_DefaultOpen))
		{
			DirectionalLight& d{ scene.light };
			ImguiFloat3("Direction", &d.lightDirection);

			ImGui::ColorEdit3("Diffuse Color", &d.diffuseColor.r);

			ImGui::NewLine();
			ImGui::ColorEdit3("Ambient Color", &d.ambientColor.r);
			ImGui::SliderFloat("Ambient Strength", &d.ambientStrength, 0.f, 10.f);

			ImGui::NewLine();
			ImGui::ColorEdit3("Specular Color", &d.specularColor.r);

			ImGui::SliderFloat("Specular Strength", &d.specularStrength, 0.f, 1.f);
			ImGui::SliderFloat("Specular Shininess", &d.specularShininess, 1.f, 256.f);
		}

		ImGui::NewLine();

		if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_CollapsingHeader | ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImguiFloat3("Position", camera.GetPosition());
			ImGui::SliderAngle("Yaw", camera.GetYaw(), -180.f, 180.f);
			ImGui::SliderAngle("Pitch", camera.GetPitch(), -89.f, 89.f);
			ImGui::TextUnformatted("Tab: toggle mouse look / cursor");
		}

		ImGui::End();
		ImGui::Render();
	}
} // namespace gg
