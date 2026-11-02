module;
#include <directx/d3d12.h>
#include <SDL2/SDL_video.h>
#include <memory>
#include <wrl.h>
export module DebugUI_D3D12;

using Microsoft::WRL::ComPtr;

export namespace gg
{
	class DebugUI_D3D12
	{
	public:
		~DebugUI_D3D12() noexcept;
		void Initialize(ComPtr<ID3D12Device4>, SDL_Window*);
		void Render(ComPtr<ID3D12GraphicsCommandList>);

	private:
		ComPtr<ID3D12DescriptorHeap> imguiSrvDescHeap;
		bool contextCreated{};
		bool sdlBackendInitialized{};
		bool dx12BackendInitialized{};
	};

} //namespace gg
