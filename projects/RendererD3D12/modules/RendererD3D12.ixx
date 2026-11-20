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
#include <wrl.h>
export module RendererD3D12;

import Camera;
import DebugOverlayD3D12;
import ErrorHandling;
import GlobalSettings;
import Input;
import Model;
import ModelD3D12;
import Renderer;
import SettingsRenderer;
import SkyboxD3D12;
import Texture;
import TimeManager;
import Vertex;

using Microsoft::WRL::ComPtr;
using std::chrono::nanoseconds;

namespace gg
{

	export class RendererD3D12 : public Renderer
	{
	public:
		RendererD3D12() = default;
		~RendererD3D12() noexcept;
		std::expected<void, Error> Initialize(RendererSettings const&) override;

		static std::shared_ptr<RendererD3D12> Get();

		RendererD3D12(RendererD3D12 const&)            = delete;
		RendererD3D12& operator=(RendererD3D12 const&) = delete;

		RendererD3D12(RendererD3D12&&) noexcept            = default;
		RendererD3D12& operator=(RendererD3D12&&) noexcept = default;

		std::shared_ptr<Camera> GetCamera() override;
		RendererType            GetType() const override;

		void                                                 OnWindowResized(uint32_t width, uint32_t height) override;
		std::expected<void, Error>                           Render(nanoseconds deltaTime) override;
		std::expected<std::shared_ptr<ShaderProgram>, Error> LoadShader(std::string const& shaderName) override;
		std::expected<void, Error>                           LoadModel(std::string const& modelRelativePath, std::string const& shaderName, glm::vec3& position) override;
		std::expected<void, Error>                           LoadSkybox(std::string const& name, std::string const& shaderName) override;

		std::expected<ComPtr<ID3D12PipelineState>, Error> CreateGraphicsPipeline(
			D3D12_SHADER_BYTECODE const& vertexShader,
			D3D12_SHADER_BYTECODE const& pixelShader,
			D3D12_INPUT_LAYOUT_DESC const&,
			D3D12_CULL_MODE,
			CD3DX12_DEPTH_STENCIL_DESC1 const&,
			ID3D12RootSignature*,
			std::wstring const& name
		);

		/* Uploads one image per array slice into a new default-heap texture and writes its SRV to srvHandle.
		   srvDimension: D3D12_SRV_DIMENSION_TEXTURE2D (1 image) or D3D12_SRV_DIMENSION_TEXTURECUBE (6 images, +X, -X, +Y, -Y, +Z, -Z). */
		std::expected<ComPtr<ID3D12Resource>, Error> UploadTexture(
			std::span<Texture const* const> images,
			D3D12_SRV_DIMENSION             srvDimension,
			D3D12_CPU_DESCRIPTOR_HANDLE     srvHandle,
			std::wstring const&             resourceName,
			std::string const&              sourceName
		);

	private:
		std::expected<void, Error> PopulateCommandList();
		std::expected<void, Error> WaitForPreviousFrame();
		std::expected<void, Error> ResizeRenderTargets();
		std::expected<void, Error> ResizeDepthBuffer();
		std::expected<void, Error> ResizeWindow();

		std::expected<void, Error> CreateGraphicsPipeline(std::shared_ptr<ModelD3D12> model);
		std::expected<void, Error> CreateVertexBuffer(std::shared_ptr<ModelD3D12>);
		std::expected<void, Error> LoadTextures(std::shared_ptr<ModelD3D12>);

		std::expected<std::shared_ptr<Texture>, Error> LoadTexture(std::string const& name, ComPtr<ID3D12DescriptorHeap> srvHeap);

		std::expected<void, Error> CreateBuffer(
			ComPtr<ID3D12GraphicsCommandList> const& commandList,
			ComPtr<ID3D12Resource>&                  gpuResource,
			ComPtr<ID3D12Resource>&                  cpuResource,
			void const*                              data,
			uint64_t                                 sizeBytes,
			std::wstring const&                      resourceName
		);

		uint32_t    width{};
		uint32_t    height{};
		HWND        windowHandle{ nullptr };
		SDL_Window* windowHandleSDL{ nullptr };

		bool isWindowResized{ true };

		D3D12_VIEWPORT viewport{};
		D3D12_RECT     scissorRect{};

		ComPtr<ID3D12Device4>             device;
		ComPtr<ID3D12CommandQueue>        commandQueue;
		ComPtr<ID3D12CommandAllocator>    commandAllocator;
		ComPtr<ID3D12GraphicsCommandList> commandList;
		ComPtr<IDXGISwapChain3>           swapChain;
		ComPtr<ID3D12RootSignature>       rootSignature;

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

		std::vector<std::shared_ptr<ModelD3D12>> models;
		std::unique_ptr<SkyboxD3D12>             skybox;
		std::shared_ptr<TimeManager>             timeManager;
		std::shared_ptr<Camera>                  camera;
		std::unique_ptr<DebugOverlayD3D12>       debugOverlay;

		/* Synchronization objects */
		ComPtr<ID3D12Fence> fence;
		uint64_t            fenceValue{ 0 };
		HANDLE              fenceEvent{ nullptr };
		// TODO: there should be two fences and two fence values
	};

} // namespace gg
