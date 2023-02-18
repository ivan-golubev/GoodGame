module;
#include <glm/ext/vector_float3.hpp>
#include <imgui.h>
#include <memory>
module DebugUI;

import SettingsRenderer;
import Camera;
import Lighting;
import Renderer;
import Application;

namespace
{
	void ImguiFloat3(char const* label, void* data)
	{
		ImGui::InputScalarN(label, ImGuiDataType_Float, data, 3, 0, 0, "%.3f", ImGuiInputTextFlags_CharsScientific);
	}

	void ImguiColorEdit3(char const* label, float input[3], glm::vec3* outData)
	{
		if (ImGui::ColorEdit3(label, input))
			*outData = glm::vec3{ input[0], input[1], input[2] };
	}

	constexpr ImVec4 rendererNameColor[2]
	{
		ImVec4(1.f, 0.f, 0.f, 1.f),
		ImVec4(0.f, 1.f, 0.f, 1.f)
	};
} // namespace

namespace gg
{
	void RenderDebugUI()
	{
		ImGui::NewFrame();
		ImGui::Begin("Renderer settings");

		std::shared_ptr<Application> app{ Application::Get() };
		std::shared_ptr<Renderer> renderer{ app->GetRenderer() };
		std::shared_ptr<Camera> camera{ renderer->GetCamera() };

		RendererType const rendererType{ renderer->GetType() };
		ImGui::TextColored(rendererNameColor[rendererType], "Renderer: %s", ToString(rendererType).c_str());

		if (ImGui::CollapsingHeader("Light settings", ImGuiTreeNodeFlags_CollapsingHeader | ImGuiTreeNodeFlags_DefaultOpen))
		{
			DirectionalLight& d{ globalDirectionalLight };
			ImguiFloat3("Direction", &d.lightDirection);

			static float diffuseColor[3]{ d.diffuseColor.r, d.diffuseColor.g, d.diffuseColor.b };
			ImguiColorEdit3("Diffuse Color", diffuseColor, &d.diffuseColor);

			ImGui::NewLine();
			static float ambientColor[3]{ d.ambientColor.r, d.ambientColor.g, d.ambientColor.b };
			ImguiColorEdit3("Ambient Color", ambientColor, &d.ambientColor);
			ImGui::SliderFloat("Ambient Strength", &d.ambientStrength, 0.f, 10.f);

			ImGui::NewLine();
			static float specularColor[3]{ d.specularColor.r, d.specularColor.g, d.specularColor.b };
			ImguiColorEdit3("Specular Color", specularColor, &d.specularColor);

			ImGui::SliderFloat("Specular Strength", &d.specularStrength, 0.f, 1.f);
			ImGui::SliderFloat("Specular Shininess", &d.specularShininess, 1.f, 256.f);
		}

		ImGui::NewLine();

		if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_CollapsingHeader | ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImguiFloat3("Position", camera->GetPosition());
			ImguiFloat3("Focus point", camera->GetFocusPoint());
		}

		ImGui::End();
		ImGui::Render();
	}
} // namespace gg
