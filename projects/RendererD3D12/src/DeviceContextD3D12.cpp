module;
#include "D3D12CheckHResult.h"
#include <cstdint>
#include <directx/d3d12.h>
#include <directx/d3dx12.h>
#include <expected>
#include <format>
#include <memory>
#include <span>
#include <string>
#include <vector>
#include <windows.h>
#include <wrl.h>
module DeviceContextD3D12;

import D3DHelpers;
import PipelineStateStream;
import SettingsD3D12;
import Texture;

using Microsoft::WRL::ComPtr;

namespace gg
{
	std::expected<std::shared_ptr<DeviceContextD3D12>, Error> DeviceContextD3D12::Create(
		ComPtr<ID3D12Device4>      device,
		ComPtr<ID3D12CommandQueue> commandQueue
	)
	{
		ComPtr<ID3D12CommandAllocator> uploadAllocator;
		HRESULT const                  allocatorResult{ device->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(&uploadAllocator)
		) };
		D3D12_CHECK_HRESULT(allocatorResult, ErrorCode::D3D12InitializationFailed, "create upload command allocator");
		SetName(uploadAllocator.Get(), L"CommandAllocator_Upload");

		/* CreateCommandList1 creates the list closed; BeginUpload() resets it */
		ComPtr<ID3D12GraphicsCommandList> uploadCommandList;
		HRESULT const                     listResult{ device->CreateCommandList1(
			0,
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			D3D12_COMMAND_LIST_FLAG_NONE,
			IID_PPV_ARGS(&uploadCommandList)
		) };
		D3D12_CHECK_HRESULT(listResult, ErrorCode::D3D12InitializationFailed, "create upload command list");
		SetName(uploadCommandList.Get(), L"CommandList_Upload");

		ComPtr<ID3D12Fence> uploadFence;
		HRESULT const       fenceResult{ device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&uploadFence)) };
		D3D12_CHECK_HRESULT(fenceResult, ErrorCode::D3D12InitializationFailed, "create upload fence");
		SetName(uploadFence.Get(), L"Fence_Upload");

		HANDLE const uploadFenceEvent{ CreateEvent(nullptr, FALSE, FALSE, nullptr) };
		if (uploadFenceEvent == nullptr)
		{
			HRESULT const eventResult{ HRESULT_FROM_WIN32(GetLastError()) };
			D3D12_CHECK_HRESULT(eventResult, ErrorCode::D3D12InitializationFailed, "create upload fence event");
		}

		return std::make_shared<DeviceContextD3D12>(
			std::move(device),
			std::move(commandQueue),
			std::move(uploadAllocator),
			std::move(uploadCommandList),
			std::move(uploadFence),
			uploadFenceEvent
		);
	}

	DeviceContextD3D12::DeviceContextD3D12(
		ComPtr<ID3D12Device4>             device,
		ComPtr<ID3D12CommandQueue>        commandQueue,
		ComPtr<ID3D12CommandAllocator>    uploadAllocator,
		ComPtr<ID3D12GraphicsCommandList> uploadCommandList,
		ComPtr<ID3D12Fence>               uploadFence,
		HANDLE                            uploadFenceEvent
	)
		: device{ std::move(device) }
		, commandQueue{ std::move(commandQueue) }
		, uploadAllocator{ std::move(uploadAllocator) }
		, uploadCommandList{ std::move(uploadCommandList) }
		, uploadFence{ std::move(uploadFence) }
		, uploadFenceEvent{ uploadFenceEvent }
	{
	}

	DeviceContextD3D12::~DeviceContextD3D12() noexcept
	{
		/* Every upload is waited for in SubmitUploadAndWait(), so nothing is in flight here */
		if (uploadFenceEvent)
			CloseHandle(uploadFenceEvent);
	}

	std::expected<void, Error> DeviceContextD3D12::BeginUpload()
	{
		HRESULT const allocatorResult{ uploadAllocator->Reset() };
		D3D12_CHECK_HRESULT(allocatorResult, ErrorCode::D3D12CommandRecordingFailed, "reset upload command allocator");
		HRESULT const listResult{ uploadCommandList->Reset(uploadAllocator.Get(), nullptr) };
		D3D12_CHECK_HRESULT(listResult, ErrorCode::D3D12CommandRecordingFailed, "reset upload command list");
		return {};
	}

	std::expected<void, Error> DeviceContextD3D12::SubmitUploadAndWait()
	{
		HRESULT const closeResult{ uploadCommandList->Close() };
		D3D12_CHECK_HRESULT(closeResult, ErrorCode::D3D12CommandRecordingFailed, "close upload command list");
		ID3D12CommandList* commandLists[]{ uploadCommandList.Get() };
		commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

		++uploadFenceValue;
		HRESULT const signalResult{ commandQueue->Signal(uploadFence.Get(), uploadFenceValue) };
		D3D12_CHECK_HRESULT(signalResult, ErrorCode::D3D12SynchronizationFailed, "signal upload fence");
		if (uploadFence->GetCompletedValue() < uploadFenceValue)
		{
			HRESULT const eventResult{ uploadFence->SetEventOnCompletion(uploadFenceValue, uploadFenceEvent) };
			D3D12_CHECK_HRESULT(eventResult, ErrorCode::D3D12SynchronizationFailed, "set upload fence completion event");
			WaitForSingleObject(uploadFenceEvent, INFINITE);
		}
		return {};
	}

	std::expected<ComPtr<ID3D12Resource>, Error> DeviceContextD3D12::UploadTexture(
		std::span<Texture const* const> images,
		D3D12_SRV_DIMENSION             srvDimension,
		D3D12_CPU_DESCRIPTOR_HANDLE     srvHandle,
		std::wstring const&             resourceName,
		std::string const&              sourceName
	)
	{
		auto makeUploadError = [&sourceName](char const* operation, HRESULT result)
		{
			return Error{
				.code    = ErrorCode::D3D12TextureUploadFailed,
				.message = std::format(
					"Failed to {} for texture '{}': HRESULT 0x{:08X}",
					operation,
					sourceName,
					static_cast<uint32_t>(result)
				),
			};
		};

		BreakIfFalse(!images.empty());
		BreakIfFalse(srvDimension == D3D12_SRV_DIMENSION_TEXTURE2D ? images.size() == 1 : true);
		BreakIfFalse(srvDimension == D3D12_SRV_DIMENSION_TEXTURECUBE ? images.size() == 6 : true);
		uint32_t const width{ images[0]->width };
		uint32_t const height{ images[0]->height };
		for (Texture const* image : images)
			BreakIfFalse(image->width == width && image->height == height);
		uint32_t const subresourceCount{ static_cast<uint32_t>(images.size()) };

		std::expected<ComPtr<ID3D12Resource>, Error> texture{ std::in_place };
		HRESULT                                      result{};
		// Describe and create a Texture2D (array) with one slice per image.
		D3D12_RESOURCE_DESC textureDesc{
			.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
			.Alignment        = 0,
			.Width            = width,
			.Height           = height,
			.DepthOrArraySize = static_cast<UINT16>(subresourceCount),
			.MipLevels        = 1,
			.Format           = DXGI_FORMAT_R8G8B8A8_UNORM,
			.SampleDesc       = { .Count = 1, .Quality = 0 },
			.Layout           = D3D12_TEXTURE_LAYOUT_UNKNOWN,
			.Flags            = D3D12_RESOURCE_FLAG_NONE,
		};

		CD3DX12_HEAP_PROPERTIES const defaultHeapProps{ D3D12_HEAP_TYPE_DEFAULT };
		result = device->CreateCommittedResource(
			&defaultHeapProps,
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&*texture)
		);
		if (FAILED(result))
			return std::unexpected(makeUploadError("create the GPU texture resource", result));
		SetName(texture->Get(), resourceName);
		uint64_t const uploadBufferSize{ GetRequiredIntermediateSize(texture->Get(), 0, subresourceCount) };

		/* Intermediate upload buffer; only needed until the copy has executed. */
		ComPtr<ID3D12Resource>        uploadResource;
		CD3DX12_HEAP_PROPERTIES const uploadHeapProps{ D3D12_HEAP_TYPE_UPLOAD };
		CD3DX12_RESOURCE_DESC const   bufferResDesc{ CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize) };
		result = device->CreateCommittedResource(
			&uploadHeapProps,
			D3D12_HEAP_FLAG_NONE,
			&bufferResDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&uploadResource)
		);
		if (FAILED(result))
			return std::unexpected(makeUploadError("create the texture upload resource", result));

		/* Record the upload only after all resource creation has succeeded. */
		if (auto const beginResult{ BeginUpload() }; !beginResult)
			return std::unexpected(beginResult.error());

		/* Copy data to the intermediate upload heap and then schedule a copy
		   from the upload heap to the texture, one subresource per image. */
		LONG_PTR const                      textureRowPitch{ width * Texture::bytesPerPixel };
		LONG_PTR const                      textureSlicePitch{ textureRowPitch * height };
		std::vector<D3D12_SUBRESOURCE_DATA> textureData;
		textureData.reserve(subresourceCount);
		for (Texture const* image : images)
			textureData.push_back(D3D12_SUBRESOURCE_DATA{
				.pData      = image->pixels.get(),
				.RowPitch   = textureRowPitch,
				.SlicePitch = textureSlicePitch,
			});

		UINT64 const uploadedBytes{ UpdateSubresources(
			uploadCommandList.Get(),
			texture->Get(),
			uploadResource.Get(),
			0,
			0,
			subresourceCount,
			textureData.data()
		) };
		if (uploadedBytes == 0)
		{
			HRESULT const closeResult{ uploadCommandList->Close() };
			if (FAILED(closeResult))
				return std::unexpected(makeUploadError("close the command list after texture upload failed", closeResult));
			return std::unexpected(Error{
				.code    = ErrorCode::D3D12TextureUploadFailed,
				.message = std::format("Failed to copy texture '{}' into the D3D12 upload resource", sourceName),
			});
		}

		CD3DX12_RESOURCE_BARRIER barrier{ CD3DX12_RESOURCE_BARRIER::Transition(
			texture->Get(),
			D3D12_RESOURCE_STATE_COPY_DEST,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
		) };
		uploadCommandList->ResourceBarrier(1, &barrier);
		{
			/* Describe and create a SRV for the texture. */
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{
				.Format                  = textureDesc.Format,
				.ViewDimension           = srvDimension,
				.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
			};
			if (srvDimension == D3D12_SRV_DIMENSION_TEXTURECUBE)
				srvDesc.TextureCube = { .MostDetailedMip = 0, .MipLevels = 1, .ResourceMinLODClamp = 0.f };
			else
				srvDesc.Texture2D = { .MostDetailedMip = 0, .MipLevels = 1, .PlaneSlice = 0, .ResourceMinLODClamp = 0.f };
			device->CreateShaderResourceView(texture->Get(), &srvDesc, srvHandle);
		}

		/* Wait for the copy to finish before the upload buffer is released. */
		if (auto const waitResult{ SubmitUploadAndWait() }; !waitResult)
			return std::unexpected(waitResult.error());

		return texture;
	}

	std::expected<void, Error> DeviceContextD3D12::UploadBuffer(
		ComPtr<ID3D12Resource>& gpuResource,
		ComPtr<ID3D12Resource>& cpuResource,
		void const*             data,
		uint64_t                sizeBytes,
		std::wstring const&     resourceName
	)
	{
		if (auto const beginResult{ BeginUpload() }; !beginResult)
			return std::unexpected(beginResult.error());

		/* create an intermediate resource */
		CD3DX12_HEAP_PROPERTIES uploadHeapProps{ D3D12_HEAP_TYPE_UPLOAD };
		CD3DX12_RESOURCE_DESC   uploadResourceProps{ CD3DX12_RESOURCE_DESC::Buffer(sizeBytes) };
		HRESULT const           d3d12CreateUploadBufferResourceResult{ device->CreateCommittedResource(
			&uploadHeapProps,
			D3D12_HEAP_FLAG_NONE,
			&uploadResourceProps,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&cpuResource)
		) };
		D3D12_CHECK_HRESULT(d3d12CreateUploadBufferResourceResult, ErrorCode::D3D12BufferCreationFailed, "create upload buffer resource");
		SetName(cpuResource.Get(), std::format(L"UploadBuffer_{}", resourceName));

		/* create the target resource on the GPU */
		CD3DX12_HEAP_PROPERTIES const defaultHeapProps{ D3D12_HEAP_TYPE_DEFAULT };
		CD3DX12_RESOURCE_DESC const   gpuResourceProps{ CD3DX12_RESOURCE_DESC::Buffer(sizeBytes, D3D12_RESOURCE_FLAG_NONE) };
		HRESULT const                 d3d12CreateGPUBufferResourceResult{ device->CreateCommittedResource(
			&defaultHeapProps,
			D3D12_HEAP_FLAG_NONE,
			&gpuResourceProps,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&gpuResource)
		) };
		D3D12_CHECK_HRESULT(d3d12CreateGPUBufferResourceResult, ErrorCode::D3D12BufferCreationFailed, "create GPU buffer resource");
		SetName(gpuResource.Get(), std::format(L"Buffer_{}", resourceName));

		/* transfer the data */
		D3D12_SUBRESOURCE_DATA subresourceData{
			.pData      = data,
			.RowPitch   = static_cast<LONG_PTR>(sizeBytes),
			.SlicePitch = static_cast<LONG_PTR>(sizeBytes),
		};

		UpdateSubresources(
			uploadCommandList.Get(),
			gpuResource.Get(),
			cpuResource.Get(),
			0, 0, 1,
			&subresourceData
		);
		return SubmitUploadAndWait();
	}

	std::expected<ComPtr<ID3D12PipelineState>, Error> DeviceContextD3D12::CreateGraphicsPipeline(
		D3D12_SHADER_BYTECODE const&       vertexShader,
		D3D12_SHADER_BYTECODE const&       pixelShader,
		D3D12_INPUT_LAYOUT_DESC const&     inputLayout,
		D3D12_CULL_MODE                    cullMode,
		CD3DX12_DEPTH_STENCIL_DESC1 const& depthStencil,
		ID3D12RootSignature*               pipelineRootSignature,
		std::wstring const&                name
	)
	{
		std::expected<ComPtr<ID3D12PipelineState>, Error> result{ std::in_place };

		D3D12_RT_FORMAT_ARRAY rtvFormats{
			.RTFormats        = { rtvFormat },
			.NumRenderTargets = 1,
		};

		CD3DX12_BLEND_DESC      blendDesc{ D3D12_DEFAULT };
		CD3DX12_RASTERIZER_DESC rasterizerDesc{ D3D12_DEFAULT };
		rasterizerDesc.CullMode = cullMode;

		PipelineStateStream pipelineStateStream{
			.rootSignature         = pipelineRootSignature,
			.inputLayout           = CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT(inputLayout),
			.primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
			.blendState            = blendDesc,
			.rasterizerState       = rasterizerDesc,
			.depthStencilState     = depthStencil,
			.VS                    = vertexShader,
			.PS                    = pixelShader,
			.DSVFormat             = DXGI_FORMAT_D32_FLOAT,
			.RTVFormats            = rtvFormats,
		};

		D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc{
			.SizeInBytes                   = sizeof(PipelineStateStream),
			.pPipelineStateSubobjectStream = &pipelineStateStream,
		};
		HRESULT const d3d12CreateGraphicsPipelineStateResult{ device->CreatePipelineState(
			&pipelineStateStreamDesc,
			IID_PPV_ARGS(&*result)
		) };
		D3D12_CHECK_HRESULT(
			d3d12CreateGraphicsPipelineStateResult,
			ErrorCode::D3D12PipelineCreationFailed,
			"create graphics pipeline state"
		);
		SetName(result->Get(), name);
		return result;
	}
} // namespace gg
