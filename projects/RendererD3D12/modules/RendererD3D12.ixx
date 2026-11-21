module;
#include <chrono>
#include <cstdint>
#include <directx/d3d12.h>
#include <directx/d3dx12.h>
#include <dxgi1_6.h>
#include <expected>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <memory>
#include <SDL2/SDL_video.h>
#include <span>
#include <string>
#include <unordered_map>
#include <wrl.h>
export module RendererD3D12;

import Camera;
import DebugOverlayD3D12;
import DeviceContextD3D12;
import ErrorHandling;
import GlobalSettings;
import Input;
import Model;
import ModelD3D12;
import Renderer;
import ObjectId;
import SettingsRenderer;
import ShaderProgram;
import SkyboxD3D12;
import TimeManager;
import Vertex;

using Microsoft::WRL::ComPtr;
namespace gg
{

	export class RendererD3D12 : public Renderer
	{
	public:
		RendererD3D12() = default;
		~RendererD3D12() noexcept;
		std::expected<void, Error> Initialize(RendererSettings const&) override;

		RendererD3D12(RendererD3D12 const&)            = delete;
		RendererD3D12& operator=(RendererD3D12 const&) = delete;

		RendererD3D12(RendererD3D12&&)            = delete;
		RendererD3D12& operator=(RendererD3D12&&) = delete;

		RendererType GetType() const override;

		void                       OnWindowResized(uint32_t width, uint32_t height) override;
		std::expected<void, Error> Render(Scene&) override;

		std::vector<Error> LoadScene(Scene const&) override;

	private:
		std::expected<std::shared_ptr<ShaderProgram>, Error> LoadShader(std::string const& shaderName);
		std::expected<std::shared_ptr<ModelD3D12>, Error>    LoadModel(SceneModel const&);
		std::expected<std::unique_ptr<SkyboxD3D12>, Error>   LoadSkybox(SceneSkybox const&);

		std::expected<void, Error> CreateDeviceContext(IDXGIFactory4*);
		std::expected<void, Error> PopulateCommandList(Scene&);
		std::expected<void, Error> WaitForPreviousFrame();
		std::expected<void, Error> ResizeRenderTargets();
		std::expected<void, Error> ResizeDepthBuffer();
		std::expected<void, Error> ResizeWindow();

		uint32_t    width{};
		uint32_t    height{};
		HWND        windowHandle{ nullptr };
		SDL_Window* windowHandleSDL{ nullptr };

		bool isWindowResized{ true };

		D3D12_VIEWPORT viewport{};
		D3D12_RECT     scissorRect{};

		std::shared_ptr<DeviceContextD3D12> context;
		ComPtr<ID3D12CommandAllocator>      commandAllocator;
		ComPtr<ID3D12GraphicsCommandList>   commandList;
		ComPtr<IDXGISwapChain3>             swapChain;
		ComPtr<ID3D12RootSignature>         rootSignature;

		/* Render Targets */
		ComPtr<ID3D12Resource>        renderTargets[maxFramesInFlight];
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandles[maxFramesInFlight];
		ComPtr<ID3D12DescriptorHeap>  renderTargetViewHeap;
		uint32_t                      rtvDescriptorSize{};
		uint32_t                      frameIndex{ 0 };

		/* Depth */
		ComPtr<ID3D12Resource>        depthBuffer;
		ComPtr<ID3D12DescriptorHeap>  depthStencilHeap;
		CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle;
		uint32_t                      dsvDescriptorSize{};

		std::unordered_map<ObjectId, std::shared_ptr<ModelD3D12>>  models; /* by scene object id */
		std::unordered_map<ObjectId, std::unique_ptr<SkyboxD3D12>> skyboxes; /* by scene object id */
		std::unique_ptr<DebugOverlayD3D12>                         debugOverlay;

		/* Synchronization objects */
		ComPtr<ID3D12Fence> fence;
		uint64_t            fenceValue{ 0 };
		HANDLE              fenceEvent{ nullptr };
		// TODO: Render waits for the GPU after every Present, so only one frame is in flight. To overlap CPU and
		// GPU, keep one command allocator and one fence value per frame slot (one fence is enough) and wait only
		// for the slot about to be reused. Shutdown and resize still flush the whole queue.
	};

} // namespace gg
