module;
#include <directx/dxgiformat.h>
#include <string>
#include <string_view>
export module SettingsD3D12;

export namespace gg
{
	constexpr std::string_view shaderExtensionD3D12{ "cso" };
	constexpr std::string_view shadersLocation{ "shaders/dxil" };

	constexpr DXGI_FORMAT rtvFormat{ DXGI_FORMAT_R8G8B8A8_UNORM };
}
