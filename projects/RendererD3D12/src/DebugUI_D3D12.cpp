module;
#include <directx/d3d12.h>
#include <imgui/backends/imgui_impl_dx12.h>
#include <imgui/backends/imgui_impl_sdl2.h>
#include <WinPixEventRuntime/pix3.h>
#include <wrl.h>
module DebugUI_D3D12;

import Camera;
import DebugUI;
import ErrorHandling;
import SettingsD3D12;
import SettingsRenderer;

using Microsoft::WRL::ComPtr;

namespace gg
{
	DebugUI_D3D12::DebugUI_D3D12(ComPtr<ID3D12Device4> device, SDL_Window* windowHandleSDL)
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::StyleColorsDark();

		{ /* Memory for Imgui's fonts */
			D3D12_DESCRIPTOR_HEAP_DESC desc{};
			desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			desc.NumDescriptors = 1;
			desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			ThrowIfFailed(
				device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&imguiSrvDescHeap))
			);
		}

		ImGui_ImplSDL2_InitForD3D(windowHandleSDL);
		ImGui_ImplDX12_Init(device.Get(), maxFramesInFlight, rtvFormat,
			imguiSrvDescHeap.Get(),
			imguiSrvDescHeap->GetCPUDescriptorHandleForHeapStart(),
			imguiSrvDescHeap->GetGPUDescriptorHandleForHeapStart());
	}

	DebugUI_D3D12::~DebugUI_D3D12()
	{
		ImGui_ImplDX12_Shutdown();
		ImGui_ImplSDL2_Shutdown();
	}

	void DebugUI_D3D12::Render(ComPtr<ID3D12GraphicsCommandList> commandList)
	{
		PIXSetMarker(commandList.Get(), PIX_COLOR_DEFAULT, L"ImGUI");

		ID3D12DescriptorHeap* ppHeaps[]{ imguiSrvDescHeap.Get() };
		commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

		ImGui_ImplDX12_NewFrame();
		ImGui_ImplSDL2_NewFrame();

		RenderDebugUI();

		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList.Get());
	}
} //namespace gg
