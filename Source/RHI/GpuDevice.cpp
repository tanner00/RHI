#include "GpuDevice.hpp"
#include "BarrierConversion.hpp"
#include "Common.hpp"

#pragma comment(lib, "d3d12")
#pragma comment(lib, "dxgi")
#pragma comment(lib, "dxguid")
#pragma comment(lib, "dxcompiler")

#include "D3D12/d3d12.h"
#include "dxc/d3d12shader.h"
#include "dxc/dxcapi.h"
#include <dxgi1_6.h>
#include <dxgidebug.h>

extern "C"
{
__declspec(dllexport) extern const uint32 D3D12SDKVersion = D3D12_PREVIEW_SDK_VERSION;
__declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\";
}

static constexpr DXGI_FORMAT SwapChainFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
static constexpr usize ResourceTableSize = 32;

GpuDevice::GpuDevice(const Platform::Window* window)
	: FrameFenceValues()
	, HandleIndex(1)
	, Buffers(ResourceTableSize, &GlobalAllocator::Get())
	, Textures(ResourceTableSize, &GlobalAllocator::Get())
	, Samplers(ResourceTableSize, &GlobalAllocator::Get())
	, Shaders(ResourceTableSize, &GlobalAllocator::Get())
	, GraphicsPipelines(ResourceTableSize, &GlobalAllocator::Get())
{
	Dxc::Init();

	IDXGIFactory7* dxgiFactory = nullptr;
	uint32 dxgiFlags = 0;
#if DEBUG
	dxgiFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
	CHECK_RESULT(CreateDXGIFactory2(dxgiFlags, IID_PPV_ARGS(&dxgiFactory)));

#if DEBUG
	ID3D12Debug6* d3d12Debug = nullptr;
	CHECK_RESULT(D3D12GetDebugInterface(IID_PPV_ARGS(&d3d12Debug)));
	d3d12Debug->EnableDebugLayer();
	d3d12Debug->SetEnableGPUBasedValidation(true);
	d3d12Debug->SetEnableAutoName(true);
	SAFE_RELEASE(d3d12Debug);

	IDXGIInfoQueue* dxgiInfoQueue = nullptr;
	CHECK_RESULT(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiInfoQueue)));
	CHECK_RESULT(dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_WARNING, true));
	CHECK_RESULT(dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true));
	CHECK_RESULT(dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true));
	SAFE_RELEASE(dxgiInfoQueue);
#endif

	ID3D12Device11* device = nullptr;
	IDXGIAdapter4* adapter = nullptr;
	for (uint32 adapterIndex = 0;
		 dxgiFactory->EnumAdapterByGpuPreference(adapterIndex, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter)) != DXGI_ERROR_NOT_FOUND;
		 ++adapterIndex)
	{
		DXGI_ADAPTER_DESC3 adapterDescription;
		CHECK_RESULT(adapter->GetDesc3(&adapterDescription));
		if (adapterDescription.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)
		{
			continue;
		}

		CHECK_RESULT(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&device)));
		SAFE_RELEASE(adapter);
		if (device)
		{
			break;
		}
	}
	VERIFY(device, "Failed to create a 12_1 capable D3D device!");
	Device = device;

#if DEBUG
	ID3D12InfoQueue1* d3d12InfoQueue = nullptr;
	CHECK_RESULT(Device->QueryInterface(IID_PPV_ARGS(&d3d12InfoQueue)));
	CHECK_RESULT(d3d12InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true));
	CHECK_RESULT(d3d12InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true));
	CHECK_RESULT(d3d12InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true));
	SAFE_RELEASE(d3d12InfoQueue);
#endif

	static constexpr D3D12_COMMAND_QUEUE_DESC graphicsQueueDescription =
	{
		.Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
		.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
		.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
		.NodeMask = 0,
	};
	CHECK_RESULT(Device->CreateCommandQueue(&graphicsQueueDescription, IID_PPV_ARGS(&GraphicsQueue)));

	const HWND windowHandle = static_cast<HWND>(window->Handle);
	static constexpr DXGI_SWAP_CHAIN_DESC1 swapChainDescription =
	{
		.Width = 0,
		.Height = 0,
		.Format = SwapChainFormat,
		.Stereo = false,
		.SampleDesc = DefaultSampleDescription,
		.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_BACK_BUFFER,
		.BufferCount = FramesInFlight,
		.Scaling = DXGI_SCALING_STRETCH,
		.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
		.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED,
		.Flags = 0,
	};
	static const constexpr DXGI_SWAP_CHAIN_FULLSCREEN_DESC* fullScreenSwapChainDescription = nullptr;
	IDXGISwapChain1* swapChain = nullptr;
	CHECK_RESULT(dxgiFactory->CreateSwapChainForHwnd(GraphicsQueue, windowHandle, &swapChainDescription, fullScreenSwapChainDescription, nullptr, &swapChain));
	CHECK_RESULT(swapChain->QueryInterface(IID_PPV_ARGS(&SwapChain)));
	SAFE_RELEASE(swapChain);

	FrameIndex = SwapChain->GetCurrentBackBufferIndex();

	CHECK_RESULT(dxgiFactory->MakeWindowAssociation(windowHandle, DXGI_MWA_NO_ALT_ENTER));
	SAFE_RELEASE(dxgiFactory);

#if !RELEASE
	CHECK_RESULT(Device->SetStablePowerState(true));
#endif

	D3D12_FEATURE_DATA_ROOT_SIGNATURE featureRootSignature =
	{
		.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1,
	};
	CHECK_RESULT(Device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureRootSignature, sizeof(featureRootSignature)));
	VERIFY(featureRootSignature.HighestVersion >= D3D_ROOT_SIGNATURE_VERSION_1_1, "D3D12: Expected Root Signature version 1.1 or higher!");

	D3D12_FEATURE_DATA_D3D12_OPTIONS12 featureEnhancedBarriers = {};
	CHECK_RESULT(Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS12, &featureEnhancedBarriers, sizeof(featureEnhancedBarriers)));
	VERIFY(featureEnhancedBarriers.EnhancedBarriersSupported, "D3D12: Expected Enhanced Barrier support!");

	D3D12_FEATURE_DATA_D3D12_OPTIONS featureResourceHeapTier = {};
	CHECK_RESULT(Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &featureResourceHeapTier, sizeof(featureResourceHeapTier)));
	VERIFY(featureResourceHeapTier.ResourceHeapTier >= D3D12_RESOURCE_HEAP_TIER_2, "D3D12: Expected Resource Heap Tier 2 or higher!");

	CHECK_RESULT(Device->CreateFence(FrameFenceValues[0], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&FrameFence)));
	++FrameFenceValues[0];

	static constexpr D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	for (ID3D12CommandAllocator*& allocator : UploadCommandAllocators)
	{
		CHECK_RESULT(Device->CreateCommandAllocator(type, IID_PPV_ARGS(&allocator)));
	}
	CHECK_RESULT(Device->CreateCommandList1(0, type, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&UploadCommandList)));

	for (usize i = 0; i < FramesInFlight; ++i)
	{
		PendingDeletes.Add(Array<IUnknown*> {});
	}

	ConstantBufferShaderResourceUnorderedAccessViewHeap.Create(Device, D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_1,
															   ViewHeapType::ConstantBufferShaderResourceUnorderedAccess, true);
	RenderTargetViewHeap.Create(Device, D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_1, ViewHeapType::RenderTarget, false);
	DepthStencilViewHeap.Create(Device, D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_1, ViewHeapType::DepthStencil, false);
	SamplerViewHeap.Create(Device, D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE, ViewHeapType::Sampler, true);
}

GpuDevice::~GpuDevice()
{
	WaitForIdle();
	ReleaseAllDeletes();

	DepthStencilViewHeap.Destroy();
	RenderTargetViewHeap.Destroy();
	SamplerViewHeap.Destroy();
	ConstantBufferShaderResourceUnorderedAccessViewHeap.Destroy();

	SAFE_RELEASE(UploadCommandList);
	for (ID3D12CommandAllocator* commandAllocator : UploadCommandAllocators)
	{
		SAFE_RELEASE(commandAllocator);
	}
	SAFE_RELEASE(FrameFence);
	SAFE_RELEASE(SwapChain);
	SAFE_RELEASE(GraphicsQueue);
	SAFE_RELEASE(Device);

	Dxc::Shutdown();

#if DEBUG
	IDXGIDebug1* dxgiDebug = nullptr;
	CHECK_RESULT(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug)));
	CHECK_RESULT(dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, static_cast<DXGI_DEBUG_RLO_FLAGS>(DXGI_DEBUG_RLO_ALL | DXGI_DEBUG_RLO_IGNORE_INTERNAL)));
#endif
}

Buffer GpuDevice::CreateBuffer(StringView name, const BufferDescription& description)
{
	const Buffer buffer = { HandleIndex++, description };
	Buffers.Add(buffer, D3D12Buffer(Device, buffer, name));
	return buffer;
}

Buffer GpuDevice::CreateBuffer(StringView name, const void* staticData, const BufferDescription& description)
{
	const Buffer buffer = { HandleIndex++, description };
	Buffers.Add(buffer, D3D12Buffer(Device, buffer, staticData, &PendingBufferUploads, name));
	return buffer;
}

Texture GpuDevice::CreateTexture(StringView name, BarrierLayout initialLayout, const TextureDescription& description, TextureResource existingResource)
{
	const Texture texture = { HandleIndex++, description };
	Textures.Add(texture, D3D12Texture(Device, texture, initialLayout, existingResource, name));
	return texture;
}

Sampler GpuDevice::CreateSampler(const SamplerDescription& description)
{
	const Sampler sampler = { HandleIndex++, description };
	Samplers.Add(sampler, D3D12Sampler(Device, sampler, &SamplerViewHeap));
	return sampler;
}

Shader GpuDevice::CreateShader(const ShaderDescription& description)
{
	const Shader shader = { HandleIndex++, description };
	Shaders.Add(shader, D3D12Shader(shader));
	return shader;
}

GraphicsPipeline GpuDevice::CreateGraphicsPipeline(StringView name, const GraphicsPipelineDescription& description)
{
	const GraphicsPipeline graphicsPipeline = { HandleIndex++, description };
	GraphicsPipelines.Add(graphicsPipeline, D3D12GraphicsPipeline(Device, graphicsPipeline, Shaders, name));
	return graphicsPipeline;
}

void GpuDevice::DestroyBuffer(Buffer* buffer)
{
	if (!buffer->IsValid())
	{
		return;
	}

	for (const BufferResource resource : Buffers[*buffer].Resources)
	{
		AddPendingDelete(resource);
	}

	Buffers.Remove(*buffer);
	buffer->Reset();
}

void GpuDevice::DestroyTexture(Texture* texture)
{
	if (!texture->IsValid())
	{
		return;
	}

	AddPendingDelete(Textures[*texture].Resource);

	Textures.Remove(*texture);
	texture->Reset();
}

void GpuDevice::DestroySampler(Sampler* sampler)
{
	if (!sampler->IsValid())
	{
		return;
	}

	Samplers.Remove(*sampler);
	sampler->Reset();
}

void GpuDevice::DestroyShader(Shader* shader)
{
	if (!shader->IsValid())
	{
		return;
	}

	const D3D12Shader& apiShader = Shaders[*shader];
	AddPendingDelete(apiShader.Blob);
	AddPendingDelete(apiShader.Reflection);

	Shaders.Remove(*shader);
	shader->Reset();
}

void GpuDevice::DestroyGraphicsPipeline(GraphicsPipeline* graphicsPipeline)
{
	if (!graphicsPipeline->IsValid())
	{
		return;
	}

	const D3D12GraphicsPipeline& apiGraphicsPipeline = GraphicsPipelines[*graphicsPipeline];
	AddPendingDelete(apiGraphicsPipeline.PipelineState);
	AddPendingDelete(apiGraphicsPipeline.RootSignature);

	GraphicsPipelines.Remove(*graphicsPipeline);
	graphicsPipeline->Reset();
}

GraphicsContext GpuDevice::CreateGraphicsContext()
{
	return GraphicsContext(this);
}

void GpuDevice::Write(const Buffer& buffer, const void* data)
{
	const D3D12Buffer& apiBuffer = Buffers[buffer];
	apiBuffer.Write(Device, buffer, data, GetFrameIndex(), &PendingBufferUploads);
}

void GpuDevice::Write(const Texture& texture, const void* data)
{
	WriteTexture(Device, texture, data, &PendingTextureUploads);
}

void GpuDevice::WriteCubemap(const Texture& texture, const Array<uint8*>& faces)
{
	WriteCubemapTexture(Device, texture, faces, &PendingTextureUploads);
}

void GpuDevice::Submit(const GraphicsContext& context)
{
	ReleaseFrameDeletes();
	FlushUploads();

	ID3D12CommandList* graphics[] = { context.GetCommandList() };
	GraphicsQueue->ExecuteCommandLists(ARRAY_COUNT(graphics), graphics);
}

void GpuDevice::Present()
{
	const uint64 frameFenceValue = FrameFenceValues[FrameIndex];
	CHECK_RESULT(SwapChain->Present(1, 0));

	CHECK_RESULT(GraphicsQueue->Signal(FrameFence, frameFenceValue));

	const uint32 newBackBufferIndex = SwapChain->GetCurrentBackBufferIndex();
	if (FrameFence->GetCompletedValue() < FrameFenceValues[newBackBufferIndex])
	{
		CHECK_RESULT(FrameFence->SetEventOnCompletion(FrameFenceValues[newBackBufferIndex], nullptr));
		WaitForSingleObjectEx(nullptr, INFINITE, false);
	}
	FrameFenceValues[newBackBufferIndex] = frameFenceValue + 1;

	FrameIndex = newBackBufferIndex;
}

void GpuDevice::WaitForIdle()
{
	const uint64 fenceValue = FrameFenceValues[FrameIndex];
	CHECK_RESULT(GraphicsQueue->Signal(FrameFence, fenceValue));

	if (FrameFence->GetCompletedValue() < fenceValue)
	{
		CHECK_RESULT(FrameFence->SetEventOnCompletion(fenceValue, nullptr));
		WaitForSingleObjectEx(nullptr, INFINITE, false);
	}

	++FrameFenceValues[FrameIndex];
}

void GpuDevice::ReleaseAllDeletes()
{
	for (Array<IUnknown*>& deletes : PendingDeletes)
	{
		for (IUnknown* resource : deletes)
		{
			SAFE_RELEASE(resource);
		}
		deletes.Clear();
	}
}

void GpuDevice::ResizeSwapChain(uint32 width, uint32 height)
{
	CHECK_RESULT(SwapChain->ResizeBuffers(FramesInFlight, width, height, DXGI_FORMAT_UNKNOWN, 0));

	FrameIndex = SwapChain->GetCurrentBackBufferIndex();
	for (uint64& frameFenceValue : FrameFenceValues)
	{
		frameFenceValue = FrameIndex;
	}

	RenderTargetViewHeap.Reset();
	DepthStencilViewHeap.Reset();
}

TextureResource GpuDevice::GetSwapChainResource(usize backBufferIndex) const
{
	ID3D12Resource2* resource = nullptr;
	CHECK_RESULT(SwapChain->GetBuffer(static_cast<uint32>(backBufferIndex), IID_PPV_ARGS(&resource)));
	return resource;
}

usize GpuDevice::GetFrameIndex() const
{
	return FrameIndex;
}

void GpuDevice::FlushUploads()
{
	if (PendingBufferUploads.IsEmpty() && PendingTextureUploads.IsEmpty())
	{
		return;
	}
	CHECK_RESULT(UploadCommandList->Reset(UploadCommandAllocators[FrameIndex], nullptr));

	for (const auto& [source, destination] : PendingTextureUploads)
	{
		const D3D12Texture& destinationTexture = Textures[destination];

		for (usize subresourceIndex = 0; subresourceIndex < destination.GetCount(); ++subresourceIndex)
		{
			const D3D12_RESOURCE_DESC1 textureDescription =
			{
				.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
				.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
				.Width = destination.GetWidth(),
				.Height = destination.GetHeight(),
				.DepthOrArraySize = static_cast<uint16>(destination.GetCount()),
				.MipLevels = 1,
				.Format = ToD3D12(destination.GetFormat()),
				.SampleDesc = DefaultSampleDescription,
				.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
				.Flags = D3D12_RESOURCE_FLAG_NONE,
				.SamplerFeedbackMipRegion = {},
			};
			static constexpr uint32 singleResource = 1;
			D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
			uint32 rowCount;
			Device->GetCopyableFootprints1(&textureDescription, static_cast<uint32>(subresourceIndex), singleResource, 0,
										   &layout, &rowCount, nullptr, nullptr);

			const D3D12_TEXTURE_COPY_LOCATION sourceLocation =
			{
				.pResource = source,
				.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
				.PlacedFootprint = layout,
			};
			const D3D12_TEXTURE_COPY_LOCATION destinationLocation =
			{
				.pResource = destinationTexture.Resource,
				.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
				.SubresourceIndex = static_cast<uint32>(subresourceIndex),
			};
			UploadCommandList->CopyTextureRegion(&destinationLocation, 0, 0, 0, &sourceLocation, nullptr);
		}

		PendingDeletes[FrameIndex].Add(source);
	}

	for (const auto& [source, destination] : PendingBufferUploads)
	{
		const D3D12Buffer& destinationBuffer = Buffers[destination];
		UploadCommandList->CopyResource(destinationBuffer.GetOnlyBufferResource(), source);

		PendingDeletes[FrameIndex].Add(source);
	}

	CHECK_RESULT(UploadCommandList->Close());

	ID3D12CommandList* upload[] = { UploadCommandList };
	GraphicsQueue->ExecuteCommandLists(ARRAY_COUNT(upload), upload);

	PendingBufferUploads.Clear();
	PendingTextureUploads.Clear();
}

void GpuDevice::ReleaseFrameDeletes()
{
	for (IUnknown* resource : PendingDeletes[FrameIndex])
	{
		SAFE_RELEASE(resource);
	}
	PendingDeletes[FrameIndex].Clear();
}

void GpuDevice::AddPendingDelete(IUnknown* pendingDelete)
{
	if (pendingDelete)
	{
		PendingDeletes[FrameIndex].Add(pendingDelete);
	}
}

void GpuDevice::EnsureShaderResourceView(const Texture& texture)
{
	static constexpr ViewType viewType = ViewType::ShaderResource;

	D3D12Texture& apiTexture = Textures[texture];
	if (apiTexture.HeapIndices[static_cast<usize>(viewType)])
	{
		return;
	}

	const usize heapIndex = ConstantBufferShaderResourceUnorderedAccessViewHeap.AllocateIndex();
	apiTexture.HeapIndices[static_cast<usize>(viewType)] = heapIndex;

	D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDescription;
	switch (texture.GetType())
	{
	case TextureType::Rectangle:
		shaderResourceViewDescription =
		{
			.Format = ToD3D12View(texture.GetFormat(), viewType),
			.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
			.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
			.Texture2D =
			{
				.MostDetailedMip = 0,
				.MipLevels = 1,
				.PlaneSlice = 0,
				.ResourceMinLODClamp = 0,
			},
		};
		break;
	case TextureType::Cubemap:
		shaderResourceViewDescription =
		{
			.Format = ToD3D12View(texture.GetFormat(), viewType),
			.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE,
			.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
			.TextureCube =
			{
				.MostDetailedMip = 0,
				.MipLevels = 1,
				.ResourceMinLODClamp = 0,
			},
		};
		break;
	default:
		CHECK(false);
		break;
	}
	Device->CreateShaderResourceView
	(
		apiTexture.Resource,
		&shaderResourceViewDescription,
		D3D12_CPU_DESCRIPTOR_HANDLE { ConstantBufferShaderResourceUnorderedAccessViewHeap.GetCpu(heapIndex) }
	);
}

void GpuDevice::EnsureRenderTargetView(const Texture& texture)
{
	static constexpr ViewType viewType = ViewType::RenderTarget;

	D3D12Texture& apiTexture = Textures[texture];
	if (apiTexture.HeapIndices[static_cast<usize>(viewType)])
	{
		return;
	}

	const usize heapIndex = RenderTargetViewHeap.AllocateIndex();
	apiTexture.HeapIndices[static_cast<usize>(viewType)] = heapIndex;

	const D3D12_RENDER_TARGET_VIEW_DESC renderTargetViewDescription =
	{
		.Format = ToD3D12View(texture.GetFormat(), viewType),
		.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D,
		.Texture2D =
		{
			.MipSlice = 0,
			.PlaneSlice = 0,
		},
	};
	Device->CreateRenderTargetView(
		apiTexture.Resource,
		&renderTargetViewDescription,
		D3D12_CPU_DESCRIPTOR_HANDLE { RenderTargetViewHeap.GetCpu(heapIndex) }
	);
}

void GpuDevice::EnsureDepthStencilView(const Texture& texture)
{
	static constexpr ViewType viewType = ViewType::DepthStencil;

	D3D12Texture& apiTexture = Textures[texture];
	if (apiTexture.HeapIndices[static_cast<usize>(viewType)])
	{
		return;
	}

	const usize heapIndex = DepthStencilViewHeap.AllocateIndex();
	apiTexture.HeapIndices[static_cast<usize>(viewType)] = heapIndex;

	const D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilViewDescription =
	{
		.Format = ToD3D12View(texture.GetFormat(), viewType),
		.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
		.Texture2D =
		{
			.MipSlice = 0,
		},
	};
	Device->CreateDepthStencilView
	(
		apiTexture.Resource,
		&depthStencilViewDescription,
		D3D12_CPU_DESCRIPTOR_HANDLE { DepthStencilViewHeap.GetCpu(heapIndex) }
	);
}

ID3D12Device11* GpuDevice::GetDevice() const
{
	CHECK(Device);
	return Device;
}

IDXGISwapChain4* GpuDevice::GetSwapChain() const
{
	CHECK(SwapChain);
	return SwapChain;
}
