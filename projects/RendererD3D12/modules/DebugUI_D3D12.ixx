module;
#include <directx/d3d12.h>
#include <SDL2/SDL_video.h>
#include <memory>
#include <wrl.h>
export module DebugUI_D3D12;

using Microsoft::WRL::ComPtr;

export namespace gg
{
	class Camera;

	class DebugUI_D3D12
	{
	public:
		DebugUI_D3D12(ComPtr<ID3D12Device4>, SDL_Window*);
		~DebugUI_D3D12();
		void Render(ComPtr<ID3D12GraphicsCommandList>, std::shared_ptr<Camera>);

	private:
		ComPtr<ID3D12DescriptorHeap> imguiSrvDescHeap;
	};

} //namespace gg
