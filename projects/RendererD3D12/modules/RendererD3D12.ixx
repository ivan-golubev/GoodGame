module;
#include <chrono>
#include <cstdint>
#include <directx/d3d12.h>
#include <directx/d3dx12.h>
#include <dxgi1_6.h>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <memory>
#include <SDL2/SDL_video.h>
#include <string>
#include <wrl.h>
export module RendererD3D12;

import Camera;
import DebugUI_D3D12;
import GlobalSettings;
import Input;
import Model;
import ModelD3D12;
import Renderer;
import SettingsRenderer;
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
		RendererD3D12(RendererSettings const&);
		~RendererD3D12();

		static std::shared_ptr<RendererD3D12> Get();

		RendererD3D12(RendererD3D12 const&) = delete;
		RendererD3D12& operator=(RendererD3D12 const&) = delete;

		RendererD3D12(RendererD3D12&&) noexcept = default;
		RendererD3D12& operator=(RendererD3D12&&) noexcept = default;

		void OnWindowResized(uint32_t width, uint32_t height) override;
		void Render(nanoseconds deltaTime) override;
		std::shared_ptr<ShaderProgram> LoadShader(std::string const& shaderName) override;
		void LoadModel(std::string const& modelRelativePath, std::string const& shaderName, glm::vec3& position) override;

	private:
		void PopulateCommandList();
		void WaitForPreviousFrame();
		void ResizeRenderTargets();
		void ResizeDepthBuffer();
		void ResizeWindow();

		void CreateGraphicsPipeline(std::shared_ptr<ModelD3D12> model);
		void CreateVertexBuffer(std::shared_ptr<ModelD3D12>);
		void LoadTextures(std::shared_ptr<ModelD3D12>);

		std::shared_ptr<Texture> LoadTexture(std::string const& name, ComPtr<ID3D12DescriptorHeap> srvHeap);

		void CreateBuffer(
			ComPtr<ID3D12GraphicsCommandList> const& commandList,
			ComPtr<ID3D12Resource>& gpuResource,
			ComPtr<ID3D12Resource>& cpuResource,
			void const* data,
			uint64_t sizeBytes,
			std::wstring const& resourceName
		);

		uint32_t width{};
		uint32_t height{};
		HWND windowHandle{ nullptr };
		SDL_Window* windowHandleSDL{ nullptr };

		bool isWindowResized{ true };

		D3D12_VIEWPORT viewport;
		D3D12_RECT scissorRect;

		ComPtr<ID3D12Device4> device;
		ComPtr<ID3D12CommandQueue> commandQueue;
		ComPtr<ID3D12CommandAllocator> commandAllocator;
		ComPtr<ID3D12GraphicsCommandList> commandList;
		ComPtr<IDXGISwapChain3> swapChain;
		ComPtr<ID3D12RootSignature> rootSignature;

		/* Render Targets */
		ComPtr<ID3D12Resource> renderTargets[maxFramesInFlight];
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandles[maxFramesInFlight];
		ComPtr<ID3D12DescriptorHeap> renderTargetViewHeap;
		uint32_t rtvDescriptorSize;
		uint32_t frameIndex{ 0 };

		/* Depth */
		ComPtr<ID3D12Resource> depthBuffer;
		ComPtr<ID3D12DescriptorHeap> depthStencilHeap;
		CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle;
		uint32_t dsvDescriptorSize;

		std::vector<std::shared_ptr<ModelD3D12>> models;
		std::shared_ptr<TimeManager> timeManager;
		std::unique_ptr<Camera> camera;
		std::unique_ptr<DebugUI_D3D12> debugUI;

		/* Synchronization objects */
		ComPtr<ID3D12Fence> fence;
		uint64_t fenceValue{ 0 };
		HANDLE fenceEvent{ nullptr };
		// TODO: there should be two fences and two fence values
	};

} // namespace gg
