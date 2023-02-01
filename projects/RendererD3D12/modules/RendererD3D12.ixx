module;
#include <cstdint>
#include <chrono>
#include <directx/d3d12.h>
#include <directx/d3dx12.h>
#include <DirectXMath.h>
#include <dxgi1_6.h>
#include <memory>
#include <string>
#include <SDL2/SDL_video.h>
#include <wrl.h>
export module RendererD3D12;

import Camera;
import Input;
import Vertex;
import TimeManager;
import Model;
import Renderer;
import Texture;

using DirectX::XMMATRIX;
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
		std::unique_ptr<ShaderProgram> LoadShader(std::string const& shaderName) override;
		std::shared_ptr<Texture> LoadTexture(std::string const& textureRelativePath) override;
		void LoadModel(std::string const& modelRelativePath, std::unique_ptr<ShaderProgram>, std::shared_ptr<Texture>) override;

	private:
		void PopulateCommandList(XMMATRIX const& mvpMatrix);
		void WaitForPreviousFrame();
		void ResizeRenderTargets();
		void ResizeDepthBuffer();
		void ResizeWindow();
		void CreateBuffer(
			ComPtr<ID3D12GraphicsCommandList> const& commandList,
			ComPtr<ID3D12Resource>& gpuResource,
			ComPtr<ID3D12Resource>& cpuResource,
			void const* data,
			uint64_t sizeBytes,
			std::wstring const& resourceName
		);

		static constexpr int8_t mFrameCount{ 2 };

		uint32_t width{};
		uint32_t height{};
		HWND windowHandle{ nullptr };

		bool mWindowResized{ true };

		D3D12_VIEWPORT mViewport;
		D3D12_RECT mScissorRect;

		ComPtr<ID3D12Device4> device;
		ComPtr<ID3D12CommandQueue> commandQueue;
		ComPtr<ID3D12CommandAllocator> commandAllocator;
		ComPtr<ID3D12GraphicsCommandList> commandList;
		ComPtr<IDXGISwapChain3> swapChain;
		ComPtr<ID3D12PipelineState> pipelineState;
		ComPtr<ID3D12RootSignature> rootSignature;

		/* Render Targets */
		ComPtr<ID3D12Resource> renderTargets[mFrameCount];
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandles[mFrameCount];
		ComPtr<ID3D12DescriptorHeap> renderTargetViewHeap;
		uint32_t rtvDescriptorSize;

		/* Depth */
		ComPtr<ID3D12Resource> depthBuffer;
		ComPtr<ID3D12DescriptorHeap> depthStencilHeap;
		CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle;
		uint32_t dsvDescriptorSize;

		/* Vertex and Index Buffers for the cube. TODO: There is a better place for them. */
		ComPtr<ID3D12Resource> VB_GPU_Resource;
		ComPtr<ID3D12Resource> VB_CPU_Resource;
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView;

		ComPtr<ID3D12Resource> IB_GPU_Resource;
		ComPtr<ID3D12Resource> IB_CPU_Resource;
		D3D12_INDEX_BUFFER_VIEW indexBufferView;

		/* Shaders */
		ComPtr<ID3DBlob> vertexShaderBlob;
		ComPtr<ID3DBlob> pixelShaderBlob;

		/* TODO: move this to a "game_object" class */
		uint32_t indexCount;

		// TODO: populate the model here
		//std::shared_ptr<ModelD3D12> model;
		std::shared_ptr<TimeManager> timeManager;
		std::unique_ptr<Camera> camera;

		/* Synchronization objects */
		ComPtr<ID3D12Fence> fence;
		uint32_t frameIndex{ 0 };
		uint64_t fenceValue{ 0 };
		HANDLE fenceEvent{ nullptr };
	};

} // namespace gg
