module;
#include <algorithm>
#include <cstdint>
#include <chrono>
#include <directx/d3d12.h>
#include <directx/d3dx12.h>
#include <dxgi1_6.h>
#include <format>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <memory>
#include <numbers>
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <vector>
#include <windows.h>
#include <WinPixEventRuntime/pix3.h>
#include <wrl.h>
module RendererD3D12;

import Application;
import Camera;
import D3DHelpers;
import DebugUI_D3D12;
import ErrorHandling;
import GlobalSettings;
import Input;
import Lighting;
import ModelD3D12;
import PipelineStateStream;
import SettingsD3D12;
import ShaderProgram;
import ShaderProgramD3D12;
import TextureD3D12;
import Vertex;

using Microsoft::WRL::ComPtr;
using std::chrono::nanoseconds;

namespace
{
	HWND getWindowHandle(SDL_Window* windowHandle)
	{
		SDL_SysWMinfo wmInfo;
		SDL_VERSION(&wmInfo.version);
		SDL_GetWindowWMInfo(windowHandle, &wmInfo);
		return wmInfo.info.win.window;
	}
}

namespace gg
{
	void RendererD3D12::Initialize(RendererSettings const& rs)
	{
		width = rs.width;
		height = rs.height;
		windowHandleSDL = rs.windowHandle;
		windowHandle = getWindowHandle(rs.windowHandle);
		timeManager = rs.timeManager;
		camera = std::make_shared<Camera>(rs.inputManager);
		scissorRect = { D3D12_DEFAULT_SCISSOR_STARTX, D3D12_DEFAULT_SCISSOR_STARTY, D3D12_VIEWPORT_BOUNDS_MAX, D3D12_VIEWPORT_BOUNDS_MAX };
		viewport = { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
		uint8_t dxgiFactoryFlags{ 0 };

		if constexpr (IsDebug())
		{ /* Enable the debug layer */
			ComPtr<ID3D12Debug1> debugController;
			if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
			{
				debugController->EnableDebugLayer();
				debugController->SetEnableGPUBasedValidation(true);
				dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
			}
		}

		ComPtr<IDXGIFactory4> factory;
		ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

		{ /* Select a hardware adapter and create a D3D12 device */
			ComPtr<IDXGIAdapter1> hardwareAdapter;
			GetHardwareAdapter(factory.Get(), &hardwareAdapter, true);

			ThrowIfFailed(D3D12CreateDevice(
				hardwareAdapter.Get(),
				D3D_FEATURE_LEVEL_11_0,
				IID_PPV_ARGS(&device)
			));
			SetName(device.Get(), L"DefaultDevice");
		}

		if constexpr (IsDebug())
		{ /* enabled debug breakpoints */
			ComPtr<ID3D12InfoQueue> infoQueue;
			if (SUCCEEDED(device.As(&infoQueue)))
			{
				infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
				infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
				infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);
			}
		}

		{ /* Create a command queue */
			D3D12_COMMAND_QUEUE_DESC queueDesc{
				.Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
				.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
			};

			ThrowIfFailed(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue)));
			SetName(commandQueue.Get(), L"MainCommandQueue");
		}

		{ /* Create a swap chain */
			DXGI_SWAP_CHAIN_DESC1 swapChainDesc{
				.Width = width,
				.Height = height,
				.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
				.SampleDesc = { .Count = 1, .Quality = 0 },
				.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
				.BufferCount = maxFramesInFlight,
				.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
			};

			ComPtr<IDXGISwapChain1> swapChain1;
			ThrowIfFailed(factory->CreateSwapChainForHwnd(
				commandQueue.Get(),
				windowHandle,
				&swapChainDesc,
				nullptr,
				nullptr,
				&swapChain1
			));
			ThrowIfFailed(swapChain1.As(&swapChain));
		}

		/* No support for fullscreen transitions. */
		ThrowIfFailed(factory->MakeWindowAssociation(windowHandle, DXGI_MWA_NO_ALT_ENTER));

		{ /* Describe and create a render target view (RTV) descriptor heap. */
			D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{
				.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
				.NumDescriptors = maxFramesInFlight,
				.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
			};
			ThrowIfFailed(device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&renderTargetViewHeap)));
			SetName(renderTargetViewHeap.Get(), L"RTV_DescriptorHeap");
			rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		}

		/* Create render targets */
		ResizeRenderTargets();

		/* Create a command allocator and a command list */
		ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator)));
		SetName(commandAllocator.Get(), L"DefaultAllocator");
		ThrowIfFailed(device->CreateCommandList1(0, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&commandList)));
		SetName(commandList.Get(), L"DefaultCommandList");
		auto name = GetName(commandList.Get());

		{ /* Create synchronization objects */
			ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
			SetName(fence.Get(), L"DefaultFence");
			fenceValue = 1;

			// Create an event handle to use for frame synchronization.
			fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
			if (fenceEvent == nullptr)
			{
				ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
			}
		}

		{ /* Describe and create a depth stencil view (DSV) descriptor heap. */
			D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc{
				.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
				.NumDescriptors = 1,
				.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
			};
			ThrowIfFailed(device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&depthStencilHeap)));

			dsvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		}
		/* Create depth buffer */
		ResizeDepthBuffer();

		{ /* create the root signature */
			/* Check for the highest supported root signature */
			// TODO: move the root signature to HLSL and precompile it
			D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData{
				.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1,
			};
			if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
			{
				featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
			}
			/* Create the root signature */
			D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
				D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
				D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
				D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
				D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

			CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
			ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

			CD3DX12_ROOT_PARAMETER1 rootParameters[3];
			rootParameters[0].InitAsConstants(sizeof(ModelViewProjectionCB) / sizeof(float), 0, 0, D3D12_SHADER_VISIBILITY_ALL);
			rootParameters[1].InitAsConstants(sizeof(DirectionalLight) / sizeof(float), 1, 0, D3D12_SHADER_VISIBILITY_PIXEL);
			rootParameters[2].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);

			D3D12_STATIC_SAMPLER_DESC sampler{
				.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR,
				.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
				.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
				.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
				.MipLODBias = 0,
				.MaxAnisotropy = 0,
				.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS,
				.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK,
				.MinLOD = 0.0f,
				.MaxLOD = D3D12_FLOAT32_MAX,
				.ShaderRegister = 0,
				.RegisterSpace = 0,
				.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL,
			};

			CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
			rootSignatureDescription.Init_1_1(_countof(rootParameters), rootParameters, 1, &sampler, rootSignatureFlags);

			/* Serialize the root signature */
			ComPtr<ID3DBlob> rootSignatureBlob;
			ComPtr<ID3DBlob> errorBlob;
			ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDescription,
				featureData.HighestVersion, &rootSignatureBlob, &errorBlob));

			// Create the root signature.
			ThrowIfFailed(device->CreateRootSignature(
				0, rootSignatureBlob->GetBufferPointer(), rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature))
			);
			SetName(rootSignature.Get(), L"RootSignature");
		}
		debugUI = std::make_unique<DebugUI_D3D12>();
		debugUI->Initialize(device, windowHandleSDL);
	}

	RendererType RendererD3D12::GetType() const
	{
		return RendererType::D3D12;
	}

	std::shared_ptr<Camera> RendererD3D12::GetCamera()
	{
		return camera;
	}

	void RendererD3D12::CreateGraphicsPipeline(std::shared_ptr<ModelD3D12> model)
	{
		BreakIfFalse(model->shaderProgram.get());
		ShaderProgramD3D12* shaderProgram = dynamic_cast<ShaderProgramD3D12*>(model->shaderProgram.get());

		/* Specify the input layout */
		D3D12_INPUT_ELEMENT_DESC const inputLayout[]{
			{ .SemanticName = "POSITION", .SemanticIndex = 0, .Format = DXGI_FORMAT_R32G32B32A32_FLOAT, .InputSlot = 0, .AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT, .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, .InstanceDataStepRate = 0 },
			{ .SemanticName = "NORMAL", .SemanticIndex = 0, .Format = DXGI_FORMAT_R32G32B32A32_FLOAT, .InputSlot = 0, .AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT, .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, .InstanceDataStepRate = 0 },
			{ .SemanticName = "TEXCOORD", .SemanticIndex = 0, .Format = DXGI_FORMAT_R32G32_FLOAT, .InputSlot = 0, .AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT, .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, .InstanceDataStepRate = 0 }
		};

		D3D12_RT_FORMAT_ARRAY rtvFormats{
			.RTFormats = { rtvFormat },
			.NumRenderTargets = 1,
		};

		CD3DX12_BLEND_DESC blendDesc{ D3D12_DEFAULT };
		CD3DX12_RASTERIZER_DESC rasterizerDesc{ D3D12_DEFAULT };

		PipelineStateStream pipelineStateStream{
			.pRootSignature = rootSignature.Get(),
			.InputLayout = CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT(D3D12_INPUT_LAYOUT_DESC{
				.pInputElementDescs = inputLayout,
				.NumElements = _countof(inputLayout),
			}),
			.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
			.BlendState = blendDesc,
			.RasterizerState = rasterizerDesc,
			.VS = CD3DX12_SHADER_BYTECODE(shaderProgram->GetVertexShader().Get()),
			.PS = CD3DX12_SHADER_BYTECODE(shaderProgram->GetFragmentShader().Get()),
			.DSVFormat = DXGI_FORMAT_D32_FLOAT,
			.RTVFormats = rtvFormats,
		};

		D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc{
			.SizeInBytes = sizeof(PipelineStateStream),
			.pPipelineStateSubobjectStream = &pipelineStateStream,
		};
		ThrowIfFailed(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&model->pipelineState)));
		std::string const psoName{ std::format("PSO_{}", model->name) };
		std::wstring const psoNameW{ psoName.begin(), psoName.end() };
		SetName(model->pipelineState.Get(), psoNameW);
	}

	std::expected<std::shared_ptr<ShaderProgram>, Error> RendererD3D12::LoadShader(std::string const& shaderName)
	{
		std::string const vertexShaderRelativePath{ std::format("{}/{}_VS.{}", shadersLocation, shaderName, shaderExtensionD3D12) };
		std::string const fragmentShaderRelativePath{ std::format("{}/{}_PS.{}", shadersLocation, shaderName, shaderExtensionD3D12) };
		std::unique_ptr<ShaderProgramD3D12> shader{ new ShaderProgramD3D12() };
		if (auto const result{ shader->Initialize(vertexShaderRelativePath, fragmentShaderRelativePath) }; !result)
			return std::unexpected(result.error());
		return std::shared_ptr<ShaderProgram>{ std::move(shader) };
	}

	std::expected<void, Error> RendererD3D12::LoadTextures(std::shared_ptr<ModelD3D12> model)
	{
		for (std::string const& textureName : model->textureNames)
		{
			auto textureResult{ LoadTexture(textureName, model->srvHeap) };
			if (!textureResult)
				return std::unexpected(textureResult.error());
			model->textures.emplace_back(std::move(*textureResult));
		}
		return {};
	}

	std::expected<std::shared_ptr<Texture>, Error> RendererD3D12::LoadTexture(std::string const& name, ComPtr<ID3D12DescriptorHeap> srvHeap)
	{
		std::string const textureRelativePath{ std::format("{}/{}.{}", texturesLocation, name, texturesExtension) };
		auto makeUploadError = [&textureRelativePath](char const* operation, HRESULT result)
		{
			return Error{
				.code = ErrorCode::D3D12TextureUploadFailed,
				.message = std::format("Failed to {} for texture '{}': HRESULT 0x{:08X}", operation, textureRelativePath, static_cast<uint32_t>(result)),
			};
		};
		std::unique_ptr<TextureD3D12> texture{ std::make_unique<TextureD3D12>(textureRelativePath) };
		std::wstring const textureName{ texture->GetName() };
		if (auto const result{ texture->Initialize() }; !result)
			return std::unexpected(result.error());

		HRESULT result{};
		// Describe and create a Texture2D.
		D3D12_RESOURCE_DESC textureDesc{
			.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
			.Alignment = 0,
			.Width = texture->width,
			.Height = texture->height,
			.DepthOrArraySize = 1,
			.MipLevels = 1,
			.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
			.SampleDesc = { .Count = 1, .Quality = 0 },
			.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
			.Flags = D3D12_RESOURCE_FLAG_NONE,
		};

		CD3DX12_HEAP_PROPERTIES const defaultHeapProps{ D3D12_HEAP_TYPE_DEFAULT };
		result = device->CreateCommittedResource(
			&defaultHeapProps,
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&texture->texture_GPU_Resource)
		);
		if (FAILED(result))
			return std::unexpected(makeUploadError("create the GPU texture resource", result));
		SetName(texture->texture_GPU_Resource.Get(), textureName);
		uint64_t const uploadBufferSize = GetRequiredIntermediateSize(texture->texture_GPU_Resource.Get(), 0, 1);

		CD3DX12_HEAP_PROPERTIES const uploadHeapProps{ D3D12_HEAP_TYPE_UPLOAD };
		CD3DX12_RESOURCE_DESC const bufferResDesc{ CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize) };
		result = device->CreateCommittedResource(
			&uploadHeapProps,
			D3D12_HEAP_FLAG_NONE,
			&bufferResDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&texture->texture_CPU_Resource));
		if (FAILED(result))
			return std::unexpected(makeUploadError("create the texture upload resource", result));

		/* Record the upload only after all resource creation has succeeded. */
		result = commandAllocator->Reset();
		if (FAILED(result))
			return std::unexpected(makeUploadError("reset the command allocator", result));
		result = commandList->Reset(commandAllocator.Get(), nullptr);
		if (FAILED(result))
			return std::unexpected(makeUploadError("reset the command list", result));

		/* Copy data to the intermediate upload heap and then schedule a copy
		   from the upload heap to the Texture2D. */
		LONG_PTR const textureRowPitch = texture->width * texture->channels;
		LONG_PTR const textureSlicePitch = textureRowPitch * texture->height;
		D3D12_SUBRESOURCE_DATA textureData{
			.pData = texture->pixels,
			.RowPitch = textureRowPitch,
			.SlicePitch = textureSlicePitch,
		};

		UINT64 const uploadedBytes{ UpdateSubresources(commandList.Get(), texture->texture_GPU_Resource.Get(), texture->texture_CPU_Resource.Get(), 0, 0, 1, &textureData) };
		if (uploadedBytes == 0)
		{
			HRESULT const closeResult{ commandList->Close() };
			if (FAILED(closeResult))
				return std::unexpected(makeUploadError("close the command list after texture upload failed", closeResult));
			return std::unexpected(Error{
				.code = ErrorCode::D3D12TextureUploadFailed,
				.message = std::format("Failed to copy texture '{}' into the D3D12 upload resource", textureRelativePath),
			});
		}

		CD3DX12_RESOURCE_BARRIER barrier{ CD3DX12_RESOURCE_BARRIER::Transition(texture->texture_GPU_Resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) };
		commandList->ResourceBarrier(1, &barrier);
		{
			/* Describe and create a SRV for the texture. */
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{
				.Format = textureDesc.Format,
				.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
				.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
				.Texture2D = { .MostDetailedMip = 0, .MipLevels = 1, .PlaneSlice = 0, .ResourceMinLODClamp = 0.0f },
			};
			device->CreateShaderResourceView(texture->texture_GPU_Resource.Get(), &srvDesc, srvHeap->GetCPUDescriptorHandleForHeapStart());
		}

		result = commandList->Close();
		if (FAILED(result))
			return std::unexpected(makeUploadError("close the texture upload command list", result));
		ID3D12CommandList* ppCommandLists[]{ commandList.Get() };
		commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

		WaitForPreviousFrame();

		return std::shared_ptr<Texture>{ std::move(texture) };
	}

	std::expected<void, Error> RendererD3D12::LoadModel(std::string const& modelRelativePath, std::string const& shaderName, glm::vec3& position)
	{
		auto shaderResult{ LoadShader(shaderName) };
		if (!shaderResult)
			return std::unexpected(shaderResult.error());
		std::shared_ptr<ShaderProgram> shader{ std::move(*shaderResult) };
		std::shared_ptr<ModelD3D12> model = std::make_shared<ModelD3D12>(modelRelativePath, shader, position);
		if (auto const result{ model->Initialize() }; !result)
			return std::unexpected(result.error());

		CreateGraphicsPipeline(model);
		CreateVertexBuffer(model);

		{ /* Create  a shader resource view (SRV) heap for textures */
			D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc{
				.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
				.NumDescriptors = 1,
				.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
			};
			ThrowIfFailed(device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&model->srvHeap)));

			std::wstring const resourceName{ model->name.begin(), model->name.end() };
			SetName(model->srvHeap.Get(), std::format(L"SRV_DescriptorHeap_{}", resourceName));
		}
		if (auto const result{ LoadTextures(model) }; !result)
			return std::unexpected(result.error());

		models.push_back(model);
		return {};
	}

	void RendererD3D12::CreateVertexBuffer(std::shared_ptr<ModelD3D12> model)
	{
		// TODO: handle multiple meshes properly, need to create multiple Vertex Buffers
		BreakIfFalse(model->meshes.size() < 2);
		Mesh const& mesh{ model->meshes[0] };

		ThrowIfFailed(commandAllocator->Reset());
		ThrowIfFailed(commandList->Reset(commandAllocator.Get(), model->pipelineState.Get()));

		uint64_t const VB_sizeBytes = mesh.VerticesSizeBytes();

		CreateBuffer(commandList, model->VB_GPU_Resource, model->VB_CPU_Resource, mesh.vertices.data(), VB_sizeBytes, L"VertexBuffer");
		ThrowIfFailed(commandList->Close());

		/* Upload Vertex and Index buffers */
		ID3D12CommandList* ppCommandLists[]{ commandList.Get() };
		commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

		WaitForPreviousFrame();

		/* Init the Vertex buffer view */
		model->vertexBufferView.BufferLocation = model->VB_GPU_Resource->GetGPUVirtualAddress();
		model->vertexBufferView.SizeInBytes = static_cast<uint32_t>(VB_sizeBytes);
		model->vertexBufferView.StrideInBytes = sizeof(Vertex);
	}

	void RendererD3D12::ResizeWindow()
	{
		viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f);
		ResizeRenderTargets();
		ResizeDepthBuffer();
		isWindowResized = false;
	}

	void RendererD3D12::ResizeRenderTargets()
	{
		for (uint32_t i{ 0 }; i < maxFramesInFlight; ++i)
			renderTargets[i].Reset();

		ThrowIfFailed(swapChain->ResizeBuffers(maxFramesInFlight, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0));

		for (uint8_t n{ 0 }; n < maxFramesInFlight; n++)
		{
			ThrowIfFailed(swapChain->GetBuffer(n, IID_PPV_ARGS(&renderTargets[n])));
			rtvHandles[n] = CD3DX12_CPU_DESCRIPTOR_HANDLE(renderTargetViewHeap->GetCPUDescriptorHandleForHeapStart(), n, rtvDescriptorSize);
			device->CreateRenderTargetView(renderTargets[n].Get(), nullptr, rtvHandles[n]);
			SetName(renderTargets[n].Get(), std::format(L"SwapChainBuffer[{}]", n));
		}
	}

	void RendererD3D12::ResizeDepthBuffer()
	{
		depthBuffer.Reset();

		D3D12_CLEAR_VALUE optimizedClearValue{
			.Format = DXGI_FORMAT_D32_FLOAT,
			.DepthStencil = { .Depth = 1.0f, .Stencil = 0 },
		};

		uint32_t const width{ std::max(this->width, 0U) };
		uint32_t const height{ std::max(this->height, 0U) };

		CD3DX12_HEAP_PROPERTIES const defaultHeapProps{ D3D12_HEAP_TYPE_DEFAULT };
		D3D12_RESOURCE_DESC const resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			DXGI_FORMAT_D32_FLOAT, width, height,
			1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
		);
		ThrowIfFailed(device->CreateCommittedResource(
			&defaultHeapProps,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&optimizedClearValue,
			IID_PPV_ARGS(&depthBuffer)
		));
		SetName(depthBuffer.Get(), std::format(L"{}_GPU", L"DepthStencilTexture"));

		/* create the DSV */
		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{
			.Format = DXGI_FORMAT_D32_FLOAT,
			.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
			.Flags = D3D12_DSV_FLAG_NONE,
			.Texture2D = { .MipSlice = 0 },
		};
		dsvHandle = { depthStencilHeap->GetCPUDescriptorHandleForHeapStart() };
		device->CreateDepthStencilView(depthBuffer.Get(), &dsvDesc, dsvHandle);
	}

	void RendererD3D12::CreateBuffer(
		ComPtr<ID3D12GraphicsCommandList> const& commandList,
		ComPtr<ID3D12Resource>& gpuResource,
		ComPtr<ID3D12Resource>& cpuResource,
		void const* data,
		uint64_t sizeBytes,
		std::wstring const& resourceName
	)
	{
		/* create an intermediate resource */
		CD3DX12_HEAP_PROPERTIES uploadHeapProps{ D3D12_HEAP_TYPE_UPLOAD };
		CD3DX12_RESOURCE_DESC uploadResourceProps{ CD3DX12_RESOURCE_DESC::Buffer(sizeBytes) };
		ThrowIfFailed(device->CreateCommittedResource(
			&uploadHeapProps,
			D3D12_HEAP_FLAG_NONE,
			&uploadResourceProps,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&cpuResource))
		);
		SetName(cpuResource.Get(), std::format(L"{}_CPU", resourceName));

		/* create the target resource on the GPU */
		CD3DX12_HEAP_PROPERTIES const defaultHeapProps{ D3D12_HEAP_TYPE_DEFAULT };
		CD3DX12_RESOURCE_DESC const gpuResourceProps{ CD3DX12_RESOURCE_DESC::Buffer(sizeBytes, D3D12_RESOURCE_FLAG_NONE) };
		ThrowIfFailed(device->CreateCommittedResource(
			&defaultHeapProps,
			D3D12_HEAP_FLAG_NONE,
			&gpuResourceProps,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&gpuResource))
		);
		SetName(gpuResource.Get(), std::format(L"{}_GPU", resourceName));

		/* transfer the data */
		D3D12_SUBRESOURCE_DATA subresourceData{
			.pData = data,
			.RowPitch = static_cast<LONG_PTR>(sizeBytes),
			.SlicePitch = static_cast<LONG_PTR>(sizeBytes),
		};

		UpdateSubresources(
			commandList.Get(),
			gpuResource.Get(),
			cpuResource.Get(),
			0, 0, 1,
			&subresourceData
		);
	}

	RendererD3D12::~RendererD3D12() noexcept
	{
		/* Ensure that the GPU is no longer referencing resources that are about to be
		 cleaned up by the destructor. */
		if (commandQueue && fence && fenceEvent)
		{
			try
			{
				WaitForPreviousFrame();
			}
			catch (...)
			{
				// Continue releasing resources if the device or fence has already failed.
			}
		}
		if (fenceEvent)
			CloseHandle(fenceEvent);

		/* destroys the associated shaders and textures */
		models.clear();

		debugUI.reset();

	}

	std::shared_ptr<RendererD3D12> RendererD3D12::Get()
	{
		std::shared_ptr<Application> app{ Application::Get() };
		std::shared_ptr<RendererD3D12> renderer{ dynamic_pointer_cast<RendererD3D12>(app->GetRenderer()) };
		return renderer;
	}

	void RendererD3D12::OnWindowResized(uint32_t width, uint32_t height)
	{
		isWindowResized = true;
		width = std::max(8u, width);
		height = std::max(8u, height);
	}

	void RendererD3D12::Render(nanoseconds deltaTime)
	{
		if (isWindowResized)
		{
			ResizeWindow();
			camera->UpdateProjectionMatrix(width, height);
		}

		camera->UpdateCamera(deltaTime);
		/* Record all the commands we need to render the scene into the command list. */
		PopulateCommandList();

		/* Execute the command list. */
		ID3D12CommandList* ppCommandLists[]{ commandList.Get() };
		commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

		/* Present the frame and inefficiently wait for the frame to render. */
		ThrowIfFailed(swapChain->Present(1, 0));
		WaitForPreviousFrame();
	}

	void RendererD3D12::WaitForPreviousFrame()
	{
		uint64_t const fencePrevValue{ fenceValue };
		ThrowIfFailed(commandQueue->Signal(fence.Get(), fencePrevValue));
		fenceValue++;

		/* Wait until the previous frame is finished */
		if (fence->GetCompletedValue() < fencePrevValue)
		{
			ThrowIfFailed(fence->SetEventOnCompletion(fencePrevValue, fenceEvent));
			WaitForSingleObject(fenceEvent, INFINITE);
		}
	}

	void RendererD3D12::PopulateCommandList()
	{
		ThrowIfFailed(commandList->Reset(commandAllocator.Get(), nullptr));

		uint8_t const frameIndex{ static_cast<uint8_t>(swapChain->GetCurrentBackBufferIndex()) };
		/* Set all the common state first */
		{
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			commandList->RSSetViewports(1, &viewport);
			commandList->RSSetScissorRects(1, &scissorRect);

			commandList->OMSetRenderTargets(1, &rtvHandles[frameIndex], true, &dsvHandle);
		}

		{
			PIXScopedEvent(commandList.Get(), PIX_COLOR(0, 0, 255), L"RenderFrame");

			/* Back buffer to be used as a Render Target */
			CD3DX12_RESOURCE_BARRIER barrier{ CD3DX12_RESOURCE_BARRIER::Transition(
				renderTargets[frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET
			) };
			commandList->ResourceBarrier(1, &barrier);

			CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle{ renderTargetViewHeap->GetCPUDescriptorHandleForHeapStart(), frameIndex, rtvDescriptorSize };
			float const clearColor[]{ 0.0f, 0.2f, 0.4f, 1.0f };
			commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
			commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

			/* Set per-model state and draw each */
			for (std::shared_ptr<ModelD3D12> model : models)
			{
				commandList->SetPipelineState(model->pipelineState.Get());
				commandList->SetGraphicsRootSignature(rootSignature.Get());

				ID3D12DescriptorHeap* ppHeaps[]{ model->srvHeap.Get() };
				commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

				ModelViewProjectionCB mvpMatrices{
					.MVP = CalculateMVP(model->translation, timeManager->GetCurrentTimeSec(), *camera),
					.ViewPosition = camera->GetPositionVec4(),
				};
				commandList->SetGraphicsRoot32BitConstants(0, sizeof(ModelViewProjectionCB) / sizeof(float), &mvpMatrices, 0);
				commandList->SetGraphicsRoot32BitConstants(1, sizeof(DirectionalLight) / sizeof(float), &globalDirectionalLight, 0);
				commandList->SetGraphicsRootDescriptorTable(2, model->srvHeap->GetGPUDescriptorHandleForHeapStart());
				commandList->IASetVertexBuffers(0, 1, &model->vertexBufferView);

				/* Record commands */
				for (auto& m : model->meshes)
					commandList->DrawInstanced(m.GetVertexCount(), 1, 0, 0);
			}

			debugUI->Render(commandList);

			/* Indicate that the back buffer will now be used to present. */
			barrier = CD3DX12_RESOURCE_BARRIER::Transition(
				renderTargets[frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT
			);
			commandList->ResourceBarrier(1, &barrier);
		}
		ThrowIfFailed(commandList->Close());
	}

} // namespace gg
