module;
#include <algorithm>
#include <cstdint>
#include <numbers>
#include <directx/d3d12.h>
#include <directx/d3dx12.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <dxgi1_6.h>
#include <format>
#include <wrl.h>
#include <vector>
#include <SDL2/SDL_syswm.h>

#include <windows.h>
#include <WinPixEventRuntime/pix3.h> // has to be the last - depends on types in windows.h
module RendererD3D12;

import Application;
import Camera;
import D3DHelpers;
import ErrorHandling;
import GlobalSettings;
import Input;
import PipelineStateStream;
import Vertex;
import ShaderProgram;
import ShaderProgramD3D12;
import ModelD3D12;

using namespace DirectX;
using Microsoft::WRL::ComPtr;
using std::chrono::nanoseconds;

namespace
{
	std::string const shaderExtensionD3D12{ "cso" };
	std::string const shadersLocation{ "shaders/dxil" };
	constexpr double cubeRotationSpeed{ 0.2 }; // meters per seconds

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
	RendererD3D12::RendererD3D12(RendererSettings const& rs)
		: width{ rs.width }
		, height{ rs.height }
		, windowHandle{ getWindowHandle(rs.windowHandle) }
		, timeManager{ rs.timeManager }
		, camera{ std::make_unique<Camera>(rs.inputManager) }
		, mScissorRect{ D3D12_DEFAULT_SCISSOR_STARTX, D3D12_DEFAULT_SCISSOR_STARTY, D3D12_VIEWPORT_BOUNDS_MAX, D3D12_VIEWPORT_BOUNDS_MAX }
		, mViewport{ 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f }
	{
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

		{ /* Create a command queue */
			D3D12_COMMAND_QUEUE_DESC queueDesc = {};
			queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
			queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

			ThrowIfFailed(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue)));
			SetName(commandQueue.Get(), L"MainCommandQueue");
		}

		{ /* Create a swap chain */
			DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
			swapChainDesc.BufferCount = mFrameCount;
			swapChainDesc.Width = width;
			swapChainDesc.Height = height;
			swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
			swapChainDesc.SampleDesc.Count = 1;

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
			D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
			rtvHeapDesc.NumDescriptors = mFrameCount;
			rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			ThrowIfFailed(device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&renderTargetViewHeap)));

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
			D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
			dsvHeapDesc.NumDescriptors = 1;
			dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
			dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			ThrowIfFailed(device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&depthStencilHeap)));

			dsvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		}
		/* Create depth buffer */
		ResizeDepthBuffer();

		/* Read shaders */
		//{
		//	ThrowIfFailed(D3DReadFileToBlob(L"shaders//colored_surface_VS.cso", &vertexShaderBlob));
		//	ThrowIfFailed(D3DReadFileToBlob(L"shaders//colored_surface_PS.cso", &pixelShaderBlob));
		//}

		// Now shaders and geometry are loaded outside of the renderer, TODO: remove this temp code

		//UploadGeometry();
		//__debugbreak();

		{ /* create the root signature */
			/* Check for the highest supported root signature */
			// TODO: move the root signature to HLSL and precompile it
			D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData{};
			featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
			if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
			{
				featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
			}
			/* Create the root signature */
			D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
				D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
				D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
				D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
				D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
				D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

			CD3DX12_ROOT_PARAMETER1 rootParameters[1];
			rootParameters[0].InitAsConstants(sizeof(XMMATRIX) / sizeof(float), 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);

			CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
			rootSignatureDescription.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr, rootSignatureFlags);

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
	}

	void RendererD3D12::CreateGraphicsPipeline()
	{
		BreakIfFalse(model->shaderProgram.get());
		ShaderProgramD3D12* shaderProgram = dynamic_cast<ShaderProgramD3D12*>(model->shaderProgram.get());

		/* Specify the input layout */
		D3D12_INPUT_ELEMENT_DESC const inputLayout[]{
			{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{"COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		D3D12_RT_FORMAT_ARRAY rtvFormats{};
		rtvFormats.NumRenderTargets = 1;
		rtvFormats.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

		CD3DX12_BLEND_DESC blendDesc{ D3D12_DEFAULT };
		CD3DX12_RASTERIZER_DESC rasterizerDesc{ D3D12_DEFAULT };

		PipelineStateStream pipelineStateStream{};
		pipelineStateStream.pRootSignature = rootSignature.Get();
		pipelineStateStream.InputLayout = { inputLayout, _countof(inputLayout) };
		pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pipelineStateStream.BlendState = blendDesc;
		pipelineStateStream.RasterizerState = rasterizerDesc;
		pipelineStateStream.VS = CD3DX12_SHADER_BYTECODE(shaderProgram->GetVertexShader().Get());
		pipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(shaderProgram->GetFragmentShader().Get());
		pipelineStateStream.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		pipelineStateStream.RTVFormats = rtvFormats;

		D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc{
			sizeof(PipelineStateStream), &pipelineStateStream
		};
		ThrowIfFailed(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&pipelineState)));
		SetName(pipelineState.Get(), L"DefaultPipelineState");
	}

	std::unique_ptr<ShaderProgram> RendererD3D12::LoadShader(std::string const& shaderName)
	{
		std::string const vertexShaderRelativePath{ std::format("{}/{}_VS.{}", shadersLocation, shaderName, shaderExtensionD3D12) };
		std::string const fragmentShaderRelativePath{ std::format("{}/{}_PS.{}", shadersLocation, shaderName, shaderExtensionD3D12) };
		ShaderProgram* shader = new ShaderProgramD3D12(vertexShaderRelativePath, fragmentShaderRelativePath);
		return std::unique_ptr<ShaderProgram>{ shader };
	}

	std::shared_ptr<Texture> RendererD3D12::LoadTexture(std::string const& textureRelativePath)
	{
		//Texture* texture = new TextureVulkan(textureRelativePath, device);
		//return std::shared_ptr<Texture>{ texture };
		//__debugbreak(); //TODO: implement
		return {};
	}

	void RendererD3D12::LoadModel(std::string const& modelRelativePath, std::unique_ptr<ShaderProgram> shader, std::shared_ptr<Texture> texture)
	{
		// TODO: load the model here, the model will contain shaders
		__debugbreak(); //TODO: implement		
		CreateGraphicsPipeline();
	}

	void RendererD3D12::ResizeWindow()
	{
		mViewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f);
		ResizeRenderTargets();
		ResizeDepthBuffer();
		mWindowResized = false;
	}

	void RendererD3D12::ResizeRenderTargets()
	{
		for (uint32_t i{ 0 }; i < mFrameCount; ++i)
			renderTargets[i].Reset();

		ThrowIfFailed(swapChain->ResizeBuffers(mFrameCount, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0));

		for (uint8_t n = 0; n < mFrameCount; n++)
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

		D3D12_CLEAR_VALUE optimizedClearValue{};
		optimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
		optimizedClearValue.DepthStencil = { 1.0f, 0 };

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
		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
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
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&gpuResource))
		);
		SetName(gpuResource.Get(), std::format(L"{}_GPU", resourceName));

		/* transfer the data */
		D3D12_SUBRESOURCE_DATA subresourceData{};
		subresourceData.pData = data;
		subresourceData.RowPitch = subresourceData.SlicePitch = sizeBytes;

		UpdateSubresources(
			commandList.Get(),
			gpuResource.Get(),
			cpuResource.Get(),
			0, 0, 1,
			&subresourceData
		);
	}

	RendererD3D12::~RendererD3D12()
	{
		/* Ensure that the GPU is no longer referencing resources that are about to be
		 cleaned up by the destructor. */
		WaitForPreviousFrame();
		CloseHandle(fenceEvent);
	}

	std::shared_ptr<RendererD3D12> RendererD3D12::Get()
	{
		std::shared_ptr<Application> app{ Application::Get() };
		std::shared_ptr<RendererD3D12> renderer{ dynamic_pointer_cast<RendererD3D12>(app->GetRenderer()) };
		return renderer;
	}

	void RendererD3D12::OnWindowResized(uint32_t width, uint32_t height)
	{
		mWindowResized = true;
		width = std::max(8u, width);
		height = std::max(8u, height);
	}

	void RendererD3D12::Render(nanoseconds deltaTime)
	{
		if (mWindowResized)
		{
			ResizeWindow();
			float windowAspectRatio{ width / static_cast<float>(height) };
			camera->UpdateProjectionMatrix(windowAspectRatio);
		}

		/* Rotate the model */
		float rotation = static_cast<float>(cubeRotationSpeed * std::numbers::pi_v<double> *timeManager->GetCurrentTimeSec());
		XMMATRIX const modelMatrix{ XMMatrixMultiply(XMMatrixRotationY(rotation), XMMatrixRotationZ(rotation)) };

		camera->UpdateCamera(deltaTime);
		XMMATRIX const& viewMatrix{ camera->GetViewMatrix() };
		XMMATRIX const& mProjectionMatrix{ camera->GetProjectionMatrix() };

		XMMATRIX mvpMatrix{ XMMatrixMultiply(modelMatrix, viewMatrix) };
		mvpMatrix = XMMatrixMultiply(mvpMatrix, mProjectionMatrix);

		/* Record all the commands we need to render the scene into the command list. */
		PopulateCommandList(mvpMatrix);

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

	void RendererD3D12::PopulateCommandList(XMMATRIX const& mvpMatrix)
	{
		//ThrowIfFailed(mCommandAllocator->Reset());
		ThrowIfFailed(commandList->Reset(commandAllocator.Get(), pipelineState.Get()));

		uint8_t const frameIndex{ static_cast<uint8_t>(swapChain->GetCurrentBackBufferIndex()) };
		/* Set all the state first */
		{
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			commandList->IASetVertexBuffers(0, 1, &model->vertexBufferView);
			commandList->IASetIndexBuffer(&model->indexBufferView);

			commandList->RSSetViewports(1, &mViewport);
			commandList->RSSetScissorRects(1, &mScissorRect);

			commandList->SetPipelineState(pipelineState.Get());
			commandList->SetGraphicsRootSignature(rootSignature.Get());
			commandList->SetGraphicsRoot32BitConstants(0, sizeof(XMMATRIX) / sizeof(float), &mvpMatrix, 0);

			commandList->OMSetRenderTargets(1, &rtvHandles[frameIndex], true, &dsvHandle);
		}

		{
			PIXScopedEvent(commandList.Get(), PIX_COLOR(0, 0, 255), L"RenderFrame");

			/* Back buffer to be used as a Render Target */
			CD3DX12_RESOURCE_BARRIER barrier{ CD3DX12_RESOURCE_BARRIER::Transition(
				renderTargets[frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET
			) };
			commandList->ResourceBarrier(1, &barrier);

			PIXSetMarker(commandList.Get(), PIX_COLOR_DEFAULT, L"SampleMarker");

			{  /* Record commands */
				CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle{ renderTargetViewHeap->GetCPUDescriptorHandleForHeapStart(), frameIndex, rtvDescriptorSize };
				float const clearColor[]{ 0.0f, 0.2f, 0.4f, 1.0f };
				commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
				commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
				__debugbreak(); // TODO: replace with DrawInstanced
				commandList->DrawIndexedInstanced(model->indexCount, 1, 0, 0, 0);
			}
			/* Indicate that the back buffer will now be used to present. */
			barrier = CD3DX12_RESOURCE_BARRIER::Transition(
				renderTargets[frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT
			);
			commandList->ResourceBarrier(1, &barrier);
		}
		ThrowIfFailed(commandList->Close());
	}

} // namespace gg
