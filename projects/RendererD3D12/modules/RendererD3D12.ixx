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
		std::unique_ptr<ShaderProgram> LoadShader(std::string const& vertexShaderRelativePath, std::string const& fragmentShaderRelativePath) override;
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

		ComPtr<ID3D12Device4> mDevice;
		ComPtr<ID3D12CommandQueue> mCommandQueue;
		ComPtr<ID3D12CommandAllocator> mCommandAllocator;
		ComPtr<ID3D12GraphicsCommandList> mCommandList;
		ComPtr<IDXGISwapChain3> mSwapChain;
		ComPtr<ID3D12PipelineState> mPipelineState;
		ComPtr<ID3D12RootSignature> mRootSignature;

		/* Render Targets */
		ComPtr<ID3D12Resource> mRenderTargets[mFrameCount];
		CD3DX12_CPU_DESCRIPTOR_HANDLE mRtvHandles[mFrameCount];
		ComPtr<ID3D12DescriptorHeap> mRenderTargetViewHeap;
		uint32_t mRtvDescriptorSize;

		/* Depth */
		ComPtr<ID3D12Resource> mDepthBuffer;
		ComPtr<ID3D12DescriptorHeap> mDepthStencilHeap;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mDsvHandle;
		uint32_t mDsvDescriptorSize;

		/* Vertex and Index Buffers for the cube. TODO: There is a better place for them. */
		ComPtr<ID3D12Resource> mVB_GPU_Resource;
		ComPtr<ID3D12Resource> mVB_CPU_Resource;
		D3D12_VERTEX_BUFFER_VIEW mVertexBufferView;

		ComPtr<ID3D12Resource> mIB_GPU_Resource;
		ComPtr<ID3D12Resource> mIB_CPU_Resource;
		D3D12_INDEX_BUFFER_VIEW mIndexBufferView;

		/* Shaders */
		ComPtr<ID3DBlob> mVertexShaderBlob;
		ComPtr<ID3DBlob> mPixelShaderBlob;

		/* TODO: move this to a "game_object" class */
		uint32_t mIndexCount;

		// TODO: populate the model here
		//std::shared_ptr<ModelD3D12> model;
		std::shared_ptr<TimeManager> timeManager;
		std::unique_ptr<Camera> camera;

		/* Synchronization objects */
		ComPtr<ID3D12Fence> mFence;
		uint32_t mFrameIndex{ 0 };
		uint64_t mFenceValue{ 0 };
		HANDLE mFenceEvent{ nullptr };
	};

} // namespace gg
