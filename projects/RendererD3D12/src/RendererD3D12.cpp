module;
#include "D3D12CheckHResult.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
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
#include <span>
#include <vector>
#include <windows.h>
#include <WinPixEventRuntime/pix3.h>
#include <wrl.h>
module RendererD3D12;

import Camera;
import D3DHelpers;
import DebugOverlayD3D12;
import DeviceContextD3D12;
import ErrorHandling;
import GlobalSettings;
import Input;
import Lighting;
import Logging;
import ModelD3D12;
import PipelineStateStream;
import SettingsD3D12;
import ShaderProgram;
import ShaderProgramD3D12;
import SkyboxD3D12;
import TextureD3D12;
import Vertex;

using Microsoft::WRL::ComPtr;
using std::chrono::nanoseconds;

namespace
{
	HWND GetWindowHandle(SDL_Window* windowHandle)
	{
		SDL_SysWMinfo wmInfo;
		SDL_VERSION(&wmInfo.version);
		SDL_GetWindowWMInfo(windowHandle, &wmInfo);
		return wmInfo.info.win.window;
	}
}

namespace gg
{
	std::expected<void, Error> RendererD3D12::Initialize(RendererSettings const& rs)
	{
		width           = rs.width;
		height          = rs.height;
		windowHandleSDL = rs.windowHandle;
		windowHandle    = GetWindowHandle(rs.windowHandle);
		timeManager     = rs.timeManager;
		camera          = std::make_shared<Camera>(rs.inputManager);
		scissorRect     = {
			D3D12_DEFAULT_SCISSOR_STARTX,
			D3D12_DEFAULT_SCISSOR_STARTY,
			D3D12_VIEWPORT_BOUNDS_MAX,
			D3D12_VIEWPORT_BOUNDS_MAX,
		};
		viewport = { 0.f, 0.f, static_cast<float>(width), static_cast<float>(height), 0.f, 1.f };
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
		HRESULT const         factoryResult{ CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)) };
		D3D12_CHECK_HRESULT(factoryResult, ErrorCode::D3D12InitializationFailed, "create DXGI factory");

		if (auto result{ CreateDeviceContext(factory.Get()) }; !result)
			return std::unexpected(result.error());

		{ /* Create a swap chain */
			DXGI_SWAP_CHAIN_DESC1 swapChainDesc{
				.Width       = width,
				.Height      = height,
				.Format      = DXGI_FORMAT_R8G8B8A8_UNORM,
				.SampleDesc  = { .Count = 1, .Quality = 0 },
				.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
				.BufferCount = maxFramesInFlight,
				.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD,
			};

			ComPtr<IDXGISwapChain1> swapChain1;
			HRESULT const           swapChainResult{ factory->CreateSwapChainForHwnd(
				context->GetCommandQueue(),
				windowHandle,
				&swapChainDesc,
				nullptr,
				nullptr,
				&swapChain1
			) };
			D3D12_CHECK_HRESULT(swapChainResult, ErrorCode::D3D12InitializationFailed, "create swap chain");
			HRESULT const swapChainInterfaceResult{ swapChain1.As(&swapChain) };
			D3D12_CHECK_HRESULT(swapChainInterfaceResult, ErrorCode::D3D12InitializationFailed, "query swap chain interface");
		}

		/* No support for fullscreen transitions. */
		HRESULT const windowAssociationResult{ factory->MakeWindowAssociation(windowHandle, DXGI_MWA_NO_ALT_ENTER) };
		D3D12_CHECK_HRESULT(windowAssociationResult, ErrorCode::D3D12InitializationFailed, "configure window association");

		{ /* Describe and create a render target view (RTV) descriptor heap. */
			D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{
				.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
				.NumDescriptors = maxFramesInFlight,
				.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
			};
			HRESULT const d3d12CreateRenderTargetDescriptorHeapResult{ context->GetDevice()->CreateDescriptorHeap(
				&rtvHeapDesc,
				IID_PPV_ARGS(&renderTargetViewHeap)
			) };
			D3D12_CHECK_HRESULT(
				d3d12CreateRenderTargetDescriptorHeapResult,
				ErrorCode::D3D12InitializationFailed,
				"create render target descriptor heap"
			);
			SetName(renderTargetViewHeap.Get(), L"RTV_DescriptorHeap_SwapChain");
			rtvDescriptorSize = context->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		}

		/* Create render targets */
		if (auto result{ ResizeRenderTargets() }; !result)
			return std::unexpected(result.error());

		/* Create a command allocator and a command list */
		HRESULT const d3d12CreateCommandAllocatorResult{ context->GetDevice()->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(&commandAllocator)
		) };
		D3D12_CHECK_HRESULT(d3d12CreateCommandAllocatorResult, ErrorCode::D3D12InitializationFailed, "create command allocator");
		SetName(commandAllocator.Get(), L"CommandAllocator_Direct");
		HRESULT const d3d12CreateCommandListResult{ context->GetDevice()->CreateCommandList1(
			0,
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			D3D12_COMMAND_LIST_FLAG_NONE,
			IID_PPV_ARGS(&commandList)
		) };
		D3D12_CHECK_HRESULT(d3d12CreateCommandListResult, ErrorCode::D3D12InitializationFailed, "create command list");
		SetName(commandList.Get(), L"CommandList_Direct");
		auto name{ GetName(commandList.Get()) };

		{ /* Create synchronization objects */
			HRESULT const d3d12CreateFenceResult{ context->GetDevice()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)) };
			D3D12_CHECK_HRESULT(d3d12CreateFenceResult, ErrorCode::D3D12InitializationFailed, "create fence");
			SetName(fence.Get(), L"Fence_Frame");
			fenceValue = 1;

			// Create an event handle to use for frame synchronization.
			fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
			if (fenceEvent == nullptr)
			{
				HRESULT const d3d12CreateFenceEventResult{ HRESULT_FROM_WIN32(GetLastError()) };
				D3D12_CHECK_HRESULT(d3d12CreateFenceEventResult, ErrorCode::D3D12InitializationFailed, "create fence event");
			}
		}

		{ /* Describe and create a depth stencil view (DSV) descriptor heap. */
			D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc{
				.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
				.NumDescriptors = 1,
				.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
			};
			HRESULT const d3d12CreateDepthStencilDescriptorHeapResult{ context->GetDevice()->CreateDescriptorHeap(
				&dsvHeapDesc,
				IID_PPV_ARGS(&depthStencilHeap)
			) };
			D3D12_CHECK_HRESULT(
				d3d12CreateDepthStencilDescriptorHeapResult,
				ErrorCode::D3D12InitializationFailed,
				"create depth stencil descriptor heap"
			);

			dsvDescriptorSize = context->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
		}
		/* Create depth buffer */
		if (auto result{ ResizeDepthBuffer() }; !result)
			return std::unexpected(result.error());

		{ /* create the root signature */
			/* Check for the highest supported root signature */
			// TODO: move the root signature to HLSL and precompile it
			D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData{
				.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1,
			};
			if (FAILED(context->GetDevice()->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
			{
				featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
			}
			/* Create the root signature */
			D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags{
				D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
				| D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS
				| D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS
				| D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS
			};

			CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
			ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

			CD3DX12_ROOT_PARAMETER1 rootParameters[3];
			rootParameters[0].InitAsConstants(sizeof(ModelViewProjection) / sizeof(float), 0, 0, D3D12_SHADER_VISIBILITY_ALL);
			rootParameters[1].InitAsConstants(sizeof(DirectionalLight) / sizeof(float), 1, 0, D3D12_SHADER_VISIBILITY_PIXEL);
			rootParameters[2].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);

			D3D12_STATIC_SAMPLER_DESC sampler{
				.Filter           = D3D12_FILTER_MIN_MAG_MIP_LINEAR,
				.AddressU         = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
				.AddressV         = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
				.AddressW         = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
				.MipLODBias       = 0,
				.MaxAnisotropy    = 0,
				.ComparisonFunc   = D3D12_COMPARISON_FUNC_ALWAYS,
				.BorderColor      = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK,
				.MinLOD           = 0.f,
				.MaxLOD           = D3D12_FLOAT32_MAX,
				.ShaderRegister   = 0,
				.RegisterSpace    = 0,
				.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL,
			};

			CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
			rootSignatureDescription.Init_1_1(_countof(rootParameters), rootParameters, 1, &sampler, rootSignatureFlags);

			/* Serialize the root signature */
			ComPtr<ID3DBlob> rootSignatureBlob;
			ComPtr<ID3DBlob> errorBlob;
			HRESULT const    d3d12SerializeRootSignatureResult{ D3DX12SerializeVersionedRootSignature(
				&rootSignatureDescription,
				featureData.HighestVersion,
				&rootSignatureBlob,
				&errorBlob
			) };
			D3D12_CHECK_HRESULT(d3d12SerializeRootSignatureResult, ErrorCode::D3D12InitializationFailed, "serialize root signature");

			// Create the root signature.
			HRESULT const d3d12CreateRootSignatureResult{ context->GetDevice()->CreateRootSignature(
				0,
				rootSignatureBlob->GetBufferPointer(),
				rootSignatureBlob->GetBufferSize(),
				IID_PPV_ARGS(&rootSignature)
			) };
			D3D12_CHECK_HRESULT(d3d12CreateRootSignatureResult, ErrorCode::D3D12InitializationFailed, "create root signature");
			SetName(rootSignature.Get(), L"RootSignature_Model");
		}
		debugOverlay = std::make_unique<DebugOverlayD3D12>();
		if (auto result{ debugOverlay->Initialize(context->GetDevice(), windowHandleSDL) }; !result)
			return std::unexpected(result.error());
		return {};
	}

	RendererType RendererD3D12::GetType() const
	{
		return RendererType::D3D12;
	}

	std::shared_ptr<Camera> RendererD3D12::GetCamera()
	{
		return camera;
	}

	std::expected<void, Error> RendererD3D12::CreateGraphicsPipeline(std::shared_ptr<ModelD3D12> model)
	{
		BreakIfFalse(model->shaderProgram.get());
		ShaderProgramD3D12* shaderProgram{ dynamic_cast<ShaderProgramD3D12*>(model->shaderProgram.get()) };

		/* Specify the input layout */
		D3D12_INPUT_ELEMENT_DESC const inputLayout[]{
			{
				.SemanticName         = "POSITION",
				.SemanticIndex        = 0,
				.Format               = DXGI_FORMAT_R32G32B32A32_FLOAT,
				.InputSlot            = 0,
				.AlignedByteOffset    = D3D12_APPEND_ALIGNED_ELEMENT,
				.InputSlotClass       = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
				.InstanceDataStepRate = 0,
			},
			{
				.SemanticName         = "NORMAL",
				.SemanticIndex        = 0,
				.Format               = DXGI_FORMAT_R32G32B32A32_FLOAT,
				.InputSlot            = 0,
				.AlignedByteOffset    = D3D12_APPEND_ALIGNED_ELEMENT,
				.InputSlotClass       = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
				.InstanceDataStepRate = 0,
			},
			{
				.SemanticName         = "TEXCOORD",
				.SemanticIndex        = 0,
				.Format               = DXGI_FORMAT_R32G32_FLOAT,
				.InputSlot            = 0,
				.AlignedByteOffset    = D3D12_APPEND_ALIGNED_ELEMENT,
				.InputSlotClass       = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
				.InstanceDataStepRate = 0,
			}
		};

		std::string const  psoName{ std::format("PSO_Model_{}", model->name) };
		std::wstring const psoNameW{ psoName.begin(), psoName.end() };

		auto result{ context->CreateGraphicsPipeline(
			CD3DX12_SHADER_BYTECODE(shaderProgram->GetVertexShader().Get()),
			CD3DX12_SHADER_BYTECODE(shaderProgram->GetFragmentShader().Get()),
			D3D12_INPUT_LAYOUT_DESC{ .pInputElementDescs = inputLayout, .NumElements = _countof(inputLayout) },
			D3D12_CULL_MODE_BACK,
			CD3DX12_DEPTH_STENCIL_DESC1{ D3D12_DEFAULT },
			rootSignature.Get(),
			psoNameW
		) };
		if (!result)
			return std::unexpected(std::move(result.error()));
		model->pipelineState = std::move(*result);
		return {};
	}

	std::expected<std::shared_ptr<ShaderProgram>, Error> RendererD3D12::LoadShader(std::string const& shaderName)
	{
		std::string const vertexShaderRelativePath{ std::format(
			"{}/{}_VS.{}",
			shadersLocation,
			shaderName,
			shaderExtensionD3D12
		) };
		std::string const fragmentShaderRelativePath{ std::format(
			"{}/{}_PS.{}",
			shadersLocation,
			shaderName,
			shaderExtensionD3D12
		) };
		auto              shader{ std::make_unique<ShaderProgramD3D12>(ShaderProgramD3D12::PassKey{}) };
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
		std::string const             textureRelativePath{ std::format("{}/{}.{}", texturesLocation, name, texturesExtension) };
		std::unique_ptr<TextureD3D12> texture{ std::make_unique<TextureD3D12>(textureRelativePath) };
		std::wstring const            textureName{ texture->GetName() };
		if (auto const result{ texture->Initialize() }; !result)
			return std::unexpected(result.error());

		std::array<Texture const*, 1> const images{ texture.get() };
		auto                                uploadResult{ context->UploadTexture(
			images,
			D3D12_SRV_DIMENSION_TEXTURE2D,
			srvHeap->GetCPUDescriptorHandleForHeapStart(),
			textureName,
			textureRelativePath
		) };
		if (!uploadResult)
			return std::unexpected(std::move(uploadResult.error()));
		texture->texture = std::move(*uploadResult);

		return std::shared_ptr<Texture>{ std::move(texture) };
	}

	std::expected<void, Error> RendererD3D12::LoadSkybox(std::string const& name, std::string const& shaderName)
	{
		auto shaderResult{ LoadShader(shaderName) };
		if (!shaderResult)
			return std::unexpected(shaderResult.error());

		std::unique_ptr<SkyboxD3D12> newSkybox{ std::make_unique<SkyboxD3D12>(name, std::move(*shaderResult), context) };
		if (auto result{ newSkybox->Initialize() }; !result)
			return std::unexpected(result.error());
		skybox = std::move(newSkybox);
		return {};
	}

	std::expected<void, Error> RendererD3D12::LoadModel(
		std::string const& modelRelativePath,
		std::string const& shaderName,
		glm::vec3&         position
	)
	{
		auto shaderResult{ LoadShader(shaderName) };
		if (!shaderResult)
			return std::unexpected(shaderResult.error());
		std::shared_ptr<ShaderProgram> shader{ std::move(*shaderResult) };
		std::shared_ptr<ModelD3D12>    model{ std::make_shared<ModelD3D12>(modelRelativePath, shader, position) };
		if (auto const result{ model->Initialize() }; !result)
			return std::unexpected(result.error());

		if (auto result{ CreateGraphicsPipeline(model) }; !result)
			return std::unexpected(result.error());
		if (auto result{ CreateVertexBuffer(model) }; !result)
			return std::unexpected(result.error());

		{ /* Create  a shader resource view (SRV) heap for textures */
			D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc{
				.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
				.NumDescriptors = 1,
				.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
			};
			HRESULT const d3d12CreateModelTextureDescriptorHeapResult{ context->GetDevice()->CreateDescriptorHeap(
				&srvHeapDesc,
				IID_PPV_ARGS(&model->srvHeap)
			) };
			D3D12_CHECK_HRESULT(
				d3d12CreateModelTextureDescriptorHeapResult,
				ErrorCode::D3D12InitializationFailed,
				"create model texture descriptor heap"
			);

			std::wstring const resourceName{ model->name.begin(), model->name.end() };
			SetName(model->srvHeap.Get(), std::format(L"SRV_DescriptorHeap_Model_{}", resourceName));
		}
		if (auto const result{ LoadTextures(model) }; !result)
			return std::unexpected(result.error());

		models.push_back(model);
		return {};
	}

	std::expected<void, Error> RendererD3D12::CreateVertexBuffer(std::shared_ptr<ModelD3D12> model)
	{
		// TODO: handle multiple meshes properly, need to create multiple Vertex Buffers
		BreakIfFalse(model->meshes.size() < 2);
		Mesh const& mesh{ model->meshes[0] };

		uint64_t const vertexBufferSizeBytes{ mesh.VerticesSizeBytes() };
		if (auto result{ context->UploadBuffer(
				model->vertexBuffer,
				model->vertexBufferUpload,
				mesh.vertices.data(),
				vertexBufferSizeBytes,
				std::format(L"Vertex_Model_{}", std::wstring{ model->name.begin(), model->name.end() })
			) };
		    !result)
			return std::unexpected(result.error());

		/* Init the Vertex buffer view */
		model->vertexBufferView.BufferLocation = model->vertexBuffer->GetGPUVirtualAddress();
		model->vertexBufferView.SizeInBytes    = static_cast<uint32_t>(vertexBufferSizeBytes);
		model->vertexBufferView.StrideInBytes  = sizeof(Vertex);
		return {};
	}

	std::expected<void, Error> RendererD3D12::ResizeWindow()
	{
		viewport = CD3DX12_VIEWPORT(0.f, 0.f, static_cast<float>(width), static_cast<float>(height), 0.f, 1.f);
		if (auto result{ ResizeRenderTargets() }; !result)
			return std::unexpected(result.error());
		if (auto result{ ResizeDepthBuffer() }; !result)
			return std::unexpected(result.error());
		isWindowResized = false;
		return {};
	}

	std::expected<void, Error> RendererD3D12::ResizeRenderTargets()
	{
		for (uint32_t i{ 0 }; i < maxFramesInFlight; ++i)
			renderTargets[i].Reset();

		HRESULT const d3d12ResizeSwapChainBuffersResult{ swapChain->ResizeBuffers(
			maxFramesInFlight,
			width,
			height,
			DXGI_FORMAT_R8G8B8A8_UNORM,
			0
		) };
		D3D12_CHECK_HRESULT(d3d12ResizeSwapChainBuffersResult, ErrorCode::D3D12ResizeFailed, "resize swap chain buffers");

		for (uint8_t n{ 0 }; n < maxFramesInFlight; n++)
		{
			HRESULT const d3d12GetResizedSwapChainBufferResult{ swapChain->GetBuffer(n, IID_PPV_ARGS(&renderTargets[n])) };
			D3D12_CHECK_HRESULT(d3d12GetResizedSwapChainBufferResult, ErrorCode::D3D12ResizeFailed, "get resized swap chain buffer");
			rtvHandles[n] = CD3DX12_CPU_DESCRIPTOR_HANDLE(renderTargetViewHeap->GetCPUDescriptorHandleForHeapStart(), n, rtvDescriptorSize);
			context->GetDevice()->CreateRenderTargetView(renderTargets[n].Get(), nullptr, rtvHandles[n]);
			SetName(renderTargets[n].Get(), std::format(L"RenderTarget_SwapChain_{}", n));
		}
		return {};
	}

	std::expected<void, Error> RendererD3D12::ResizeDepthBuffer()
	{
		depthBuffer.Reset();

		D3D12_CLEAR_VALUE optimizedClearValue{
			.Format       = DXGI_FORMAT_D32_FLOAT,
			.DepthStencil = { .Depth = 1.f, .Stencil = 0 },
		};

		uint32_t const width{ std::max(this->width, 0U) };
		uint32_t const height{ std::max(this->height, 0U) };

		CD3DX12_HEAP_PROPERTIES const defaultHeapProps{ D3D12_HEAP_TYPE_DEFAULT };
		D3D12_RESOURCE_DESC const     resourceDesc{ CD3DX12_RESOURCE_DESC::Tex2D(
			DXGI_FORMAT_D32_FLOAT, width, height,
			1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
		) };
		HRESULT const                 d3d12CreateDepthBufferResourceResult{ context->GetDevice()->CreateCommittedResource(
			&defaultHeapProps,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&optimizedClearValue,
			IID_PPV_ARGS(&depthBuffer)
		) };
		D3D12_CHECK_HRESULT(d3d12CreateDepthBufferResourceResult, ErrorCode::D3D12ResizeFailed, "create depth buffer resource");
		SetName(depthBuffer.Get(), L"Texture_DepthStencil");

		/* create the DSV */
		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{
			.Format        = DXGI_FORMAT_D32_FLOAT,
			.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
			.Flags         = D3D12_DSV_FLAG_NONE,
			.Texture2D     = { .MipSlice = 0 },
		};
		dsvHandle = { depthStencilHeap->GetCPUDescriptorHandleForHeapStart() };
		context->GetDevice()->CreateDepthStencilView(depthBuffer.Get(), &dsvDesc, dsvHandle);
		return {};
	}

	RendererD3D12::~RendererD3D12() noexcept
	{
		/* Ensure that the GPU is no longer referencing resources that are about to be
		 cleaned up by the destructor. */
		if (context && fence && fenceEvent)
		{
			if (auto const result{ WaitForPreviousFrame() }; !result)
				DebugLog(DebugLevel::Error, result.error().message);
		}
		if (fenceEvent)
			CloseHandle(fenceEvent);

		/* destroys the associated shaders and textures */
		models.clear();
		skybox.reset();

		debugOverlay.reset();

		/* The renderer holds the last reference to the context */
		BreakIfFalse(!context || context.use_count() == 1);
		context.reset();
	}

	void RendererD3D12::OnWindowResized(uint32_t width, uint32_t height)
	{
		isWindowResized = true;
		width           = std::max(8u, width);
		height          = std::max(8u, height);
	}

	std::expected<void, Error> RendererD3D12::Render(nanoseconds deltaTime)
	{
		if (isWindowResized)
		{
			if (auto result{ ResizeWindow() }; !result)
				return std::unexpected(result.error());
			camera->UpdateProjectionMatrix(width, height);
		}

		camera->UpdateCamera(deltaTime);
		/* Record all the commands we need to render the scene into the command list. */
		if (auto result{ PopulateCommandList() }; !result)
			return std::unexpected(result.error());

		/* Execute the command list. */
		ID3D12CommandList* commandLists[]{ commandList.Get() };
		context->GetCommandQueue()->ExecuteCommandLists(_countof(commandLists), commandLists);

		/* Present the frame and inefficiently wait for the frame to render. */
		HRESULT const d3d12PresentFrameResult{ swapChain->Present(1, 0) };
		D3D12_CHECK_HRESULT(d3d12PresentFrameResult, ErrorCode::D3D12PresentationFailed, "present frame");
		if (auto result{ WaitForPreviousFrame() }; !result)
			return std::unexpected(result.error());
		return {};
	}

	std::expected<void, Error> RendererD3D12::WaitForPreviousFrame()
	{
		uint64_t const fencePrevValue{ fenceValue };
		HRESULT const  d3d12SignalFrameFenceResult{ context->GetCommandQueue()->Signal(fence.Get(), fencePrevValue) };
		D3D12_CHECK_HRESULT(d3d12SignalFrameFenceResult, ErrorCode::D3D12SynchronizationFailed, "signal frame fence");
		fenceValue++;

		/* Wait until the previous frame is finished */
		if (fence->GetCompletedValue() < fencePrevValue)
		{
			HRESULT const d3d12SetFenceCompletionEventResult{ fence->SetEventOnCompletion(fencePrevValue, fenceEvent) };
			D3D12_CHECK_HRESULT(d3d12SetFenceCompletionEventResult, ErrorCode::D3D12SynchronizationFailed, "set fence completion event");
			WaitForSingleObject(fenceEvent, INFINITE);
		}
		return {};
	}

	std::expected<void, Error> RendererD3D12::CreateDeviceContext(IDXGIFactory4* factory)
	{
		/* Created here and handed to the context, which owns them from then on */
		ComPtr<ID3D12Device4>      device;
		ComPtr<ID3D12CommandQueue> commandQueue;

		{ /* Select a hardware adapter and create a D3D12 device */
			ComPtr<IDXGIAdapter1> const hardwareAdapter{ GetHardwareAdapter(factory, true) };

			HRESULT const deviceResult{ D3D12CreateDevice(
				hardwareAdapter.Get(),
				D3D_FEATURE_LEVEL_11_0,
				IID_PPV_ARGS(&device)
			) };
			D3D12_CHECK_HRESULT(deviceResult, ErrorCode::D3D12InitializationFailed, "create D3D12 device");
			SetName(device.Get(), L"Device");
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
				.Type  = D3D12_COMMAND_LIST_TYPE_DIRECT,
				.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
			};

			HRESULT const commandQueueResult{ device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue)) };
			D3D12_CHECK_HRESULT(commandQueueResult, ErrorCode::D3D12InitializationFailed, "create command queue");
			SetName(commandQueue.Get(), L"CommandQueue_Direct");

			auto contextResult{ DeviceContextD3D12::Create(device, commandQueue) };
			if (!contextResult)
				return std::unexpected(contextResult.error());
			context = std::move(*contextResult);
		}
		return {};
	}

	std::expected<void, Error> RendererD3D12::PopulateCommandList()
	{
		HRESULT const d3d12ResetRenderCommandListResult{ commandList->Reset(commandAllocator.Get(), nullptr) };
		D3D12_CHECK_HRESULT(d3d12ResetRenderCommandListResult, ErrorCode::D3D12CommandRecordingFailed, "reset render command list");

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

			CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle{
				renderTargetViewHeap->GetCPUDescriptorHandleForHeapStart(),
				frameIndex,
				rtvDescriptorSize,
			};
			float const clearColor[]{ 0.f, 0.2f, 0.4f, 1.f };
			commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
			commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);

			/* Set per-model state and draw each */
			for (std::shared_ptr<ModelD3D12> model : models)
			{
				commandList->SetPipelineState(model->pipelineState.Get());
				commandList->SetGraphicsRootSignature(rootSignature.Get());

				ID3D12DescriptorHeap* heaps[]{ model->srvHeap.Get() };
				commandList->SetDescriptorHeaps(_countof(heaps), heaps);

				ModelViewProjection mvpMatrices{
					CalculateModelViewProjection(model->translation, timeManager->GetCurrentTimeSec(), *camera)
				};
				commandList->SetGraphicsRoot32BitConstants(0, sizeof(ModelViewProjection) / sizeof(float), &mvpMatrices, 0);
				commandList->SetGraphicsRoot32BitConstants(1, sizeof(DirectionalLight) / sizeof(float), &globalDirectionalLight, 0);
				commandList->SetGraphicsRootDescriptorTable(2, model->srvHeap->GetGPUDescriptorHandleForHeapStart());
				commandList->IASetVertexBuffers(0, 1, &model->vertexBufferView);

				/* Record commands */
				for (auto& m : model->meshes)
					commandList->DrawInstanced(m.GetVertexCount(), 1, 0, 0);
			}

			/* After the models: the sky only fills pixels the depth test leaves at the far plane. */
			if (skybox)
				skybox->Draw(commandList.Get(), *camera);

			debugOverlay->Render(commandList, *this);

			/* Indicate that the back buffer will now be used to present. */
			barrier = CD3DX12_RESOURCE_BARRIER::Transition(
				renderTargets[frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT
			);
			commandList->ResourceBarrier(1, &barrier);
		}
		HRESULT const d3d12CloseRenderCommandListResult{ commandList->Close() };
		D3D12_CHECK_HRESULT(d3d12CloseRenderCommandListResult, ErrorCode::D3D12CommandRecordingFailed, "close render command list");
		return {};
	}

} // namespace gg

#undef D3D12_CHECK_HRESULT
