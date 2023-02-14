module;
#include <directx/dxgiformat.h>
#include <string>
export module SettingsD3D12;

export namespace gg
{
	std::string const shaderExtensionD3D12{ "cso" };
	std::string const shadersLocation{ "shaders/dxil" };

	constexpr DXGI_FORMAT rtvFormat{ DXGI_FORMAT_R8G8B8A8_UNORM };
}