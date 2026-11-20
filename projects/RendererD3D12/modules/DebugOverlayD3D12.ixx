module;
#include <directx/d3d12.h>
#include <expected>
#include <memory>
#include <SDL2/SDL_video.h>
#include <wrl.h>
export module DebugOverlayD3D12;

import ErrorHandling;
import Renderer;

using Microsoft::WRL::ComPtr;

export namespace gg
{
	class DebugOverlayD3D12
	{
	public:
		~DebugOverlayD3D12() noexcept;
		std::expected<void, Error> Initialize(ComPtr<ID3D12Device4>, SDL_Window*);
		void                       Render(ComPtr<ID3D12GraphicsCommandList>, Renderer&);

	private:
		ComPtr<ID3D12DescriptorHeap> imguiSrvDescHeap;
		bool                         contextCreated{};
		bool                         sdlBackendInitialized{};
		bool                         dx12BackendInitialized{};
	};

} // namespace gg
