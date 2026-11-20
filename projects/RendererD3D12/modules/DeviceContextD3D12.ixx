module;
#include <cstdint>
#include <directx/d3d12.h>
#include <directx/d3dx12.h>
#include <expected>
#include <memory>
#include <span>
#include <string>
#include <windows.h>
#include <wrl.h>
export module DeviceContextD3D12;

export import ErrorHandling;
import Texture;

using Microsoft::WRL::ComPtr;

export namespace gg
{
	/* The D3D12 device and the services resources need to create and upload GPU objects.
	   Shared by the renderer and the resources that need it. Uploads use their own command list and
	   fence, separate from the frame's, and run on the same queue so frames see the uploaded data. */
	class DeviceContextD3D12
	{
	public:
		static std::expected<std::shared_ptr<DeviceContextD3D12>, Error> Create(ComPtr<ID3D12Device4>, ComPtr<ID3D12CommandQueue>);

		/* Only Create() can make a PassKey */
		class PassKey
		{
			friend class DeviceContextD3D12;
			PassKey() = default;
		};
		DeviceContextD3D12(
			PassKey,
			ComPtr<ID3D12Device4>,
			ComPtr<ID3D12CommandQueue>,
			ComPtr<ID3D12CommandAllocator>,
			ComPtr<ID3D12GraphicsCommandList>,
			ComPtr<ID3D12Fence>,
			HANDLE uploadFenceEvent
		);
		~DeviceContextD3D12() noexcept;

		DeviceContextD3D12(DeviceContextD3D12 const&)            = delete;
		DeviceContextD3D12& operator=(DeviceContextD3D12 const&) = delete;
		DeviceContextD3D12(DeviceContextD3D12&&)                 = delete;
		DeviceContextD3D12& operator=(DeviceContextD3D12&&)      = delete;

		ID3D12Device4*      GetDevice() const { return device.Get(); }
		ID3D12CommandQueue* GetCommandQueue() const { return commandQueue.Get(); }

		/* Uploads one image per array slice into a new default-heap texture and writes its SRV to srvHandle.
		   srvDimension: D3D12_SRV_DIMENSION_TEXTURE2D (1 image) or D3D12_SRV_DIMENSION_TEXTURECUBE (6 images, +X, -X, +Y, -Y, +Z, -Z). */
		std::expected<ComPtr<ID3D12Resource>, Error> UploadTexture(
			std::span<Texture const* const> images,
			D3D12_SRV_DIMENSION             srvDimension,
			D3D12_CPU_DESCRIPTOR_HANDLE     srvHandle,
			std::wstring const&             resourceName,
			std::string const&              sourceName
		);

		/* Creates a default-heap buffer and its upload buffer, copies data into it and waits for the copy */
		std::expected<void, Error> UploadBuffer(
			ComPtr<ID3D12Resource>& gpuResource,
			ComPtr<ID3D12Resource>& cpuResource,
			void const*             data,
			uint64_t                sizeBytes,
			std::wstring const&     resourceName
		);

		std::expected<ComPtr<ID3D12PipelineState>, Error> CreateGraphicsPipeline(
			D3D12_SHADER_BYTECODE const& vertexShader,
			D3D12_SHADER_BYTECODE const& pixelShader,
			D3D12_INPUT_LAYOUT_DESC const&,
			D3D12_CULL_MODE,
			CD3DX12_DEPTH_STENCIL_DESC1 const&,
			ID3D12RootSignature*,
			std::wstring const& name
		);

	private:
		std::expected<void, Error> BeginUpload();
		std::expected<void, Error> SubmitUploadAndWait();

		ComPtr<ID3D12Device4>             device;
		ComPtr<ID3D12CommandQueue>        commandQueue;
		ComPtr<ID3D12CommandAllocator>    uploadAllocator;
		ComPtr<ID3D12GraphicsCommandList> uploadCommandList;
		ComPtr<ID3D12Fence>               uploadFence;
		uint64_t                          uploadFenceValue{ 0 };
		HANDLE                            uploadFenceEvent{ nullptr };
	};
} // namespace gg
