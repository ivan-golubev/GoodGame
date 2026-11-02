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
	void DebugUI_D3D12::Initialize(ComPtr<ID3D12Device4> device, SDL_Window* windowHandleSDL)
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		contextCreated = true;
		ImGui::StyleColorsDark();

		{ /* Memory for Imgui's fonts */
			D3D12_DESCRIPTOR_HEAP_DESC desc{
				.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
				.NumDescriptors = 1,
				.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
			};
			ThrowIfFailed(
				device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&imguiSrvDescHeap))
			);
		}

		sdlBackendInitialized = ImGui_ImplSDL2_InitForD3D(windowHandleSDL);
		dx12BackendInitialized = ImGui_ImplDX12_Init(device.Get(), maxFramesInFlight, rtvFormat,
			imguiSrvDescHeap.Get(),
			imguiSrvDescHeap->GetCPUDescriptorHandleForHeapStart(),
			imguiSrvDescHeap->GetGPUDescriptorHandleForHeapStart());
	}

	DebugUI_D3D12::~DebugUI_D3D12()
	{
		if (dx12BackendInitialized)
			ImGui_ImplDX12_Shutdown();
		if (sdlBackendInitialized)
			ImGui_ImplSDL2_Shutdown();
		if (contextCreated)
			ImGui::DestroyContext();
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
