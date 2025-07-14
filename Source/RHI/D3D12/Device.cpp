#include "Device.hpp"
#include "AccelerationStructure.hpp"
#include "Base.hpp"
#include "BufferView.hpp"
#include "ComputePipeline.hpp"
#include "Convert.hpp"
#include "GraphicsContext.hpp"
#include "GraphicsPipeline.hpp"
#include "Resource.hpp"
#include "Sampler.hpp"
#include "Shader.hpp"
#include "TextureView.hpp"

#include "Luft/Error.hpp"

#include <dxgi1_6.h>
#include <dxgidebug.h>

#pragma comment(lib, "d3d12")
#pragma comment(lib, "dxgi")
#pragma comment(lib, "dxguid")
#pragma comment(lib, "dxcompiler")

extern "C"
{
__declspec(dllexport) extern const uint32 D3D12SDKVersion = D3D12_PREVIEW_SDK_VERSION;
__declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\";
}

namespace RHI::D3D12
{

static constexpr DXGI_FORMAT SwapChainFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

Device::Device(const Platform::Window* window)
	: FrameFenceValues()
	, PendingDestroys(Allocator)
	, PendingUploads(Allocator)
{
	Dxc::Init();

	IDXGIFactory7* dxgiFactory = nullptr;
	uint32 dxgiFlags = 0;
#if DEBUG
	dxgiFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
	CHECK_RESULT(CreateDXGIFactory2(dxgiFlags, IID_PPV_ARGS(&dxgiFactory)));

#if DEBUG
	ID3D12Debug6* debug = nullptr;
	CHECK_RESULT(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));
	debug->EnableDebugLayer();
	debug->SetEnableGPUBasedValidation(true);
	debug->SetEnableAutoName(true);
	SAFE_RELEASE(debug);

	IDXGIInfoQueue* dxgiInfoQueue = nullptr;
	CHECK_RESULT(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiInfoQueue)));
	CHECK_RESULT(dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_WARNING, true));
	CHECK_RESULT(dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true));
	CHECK_RESULT(dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true));
	SAFE_RELEASE(dxgiInfoQueue);
#endif

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

		CHECK_RESULT(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_2, IID_PPV_ARGS(&Native)));
		SAFE_RELEASE(adapter);
		if (Native)
		{
			break;
		}
	}
	VERIFY(Native, "Failed to create a 12_2 capable D3D device!");

#if DEBUG
	ID3D12InfoQueue1* d3d12InfoQueue = nullptr;
	CHECK_RESULT(Native->QueryInterface(IID_PPV_ARGS(&d3d12InfoQueue)));
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
	CHECK_RESULT(Native->CreateCommandQueue(&graphicsQueueDescription, IID_PPV_ARGS(&GraphicsQueue)));

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

	CHECK_RESULT(dxgiFactory->MakeWindowAssociation(windowHandle, DXGI_MWA_NO_ALT_ENTER));
	SAFE_RELEASE(dxgiFactory);

#if !RELEASE
	CHECK_RESULT(Native->SetStablePowerState(true));
#endif

	D3D12_FEATURE_DATA_D3D12_OPTIONS12 featureEnhancedBarriers = {};
	CHECK_RESULT(Native->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS12, &featureEnhancedBarriers, sizeof(featureEnhancedBarriers)));
	VERIFY(featureEnhancedBarriers.EnhancedBarriersSupported, "D3D12: Expected Enhanced Barrier support!");

	D3D12_FEATURE_DATA_SHADER_MODEL featureShaderModel = { D3D_SHADER_MODEL_6_6 };
	CHECK_RESULT(Native->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &featureShaderModel, sizeof(featureShaderModel)));
	VERIFY(featureShaderModel.HighestShaderModel >= D3D_SHADER_MODEL_6_6, "D3D12: Expected Shader Model 6.6 support!");

	CHECK_RESULT(Native->CreateFence(FrameFenceValues[0], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&FrameFence)));
	++FrameFenceValues[0];

	static constexpr D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	for (ID3D12CommandAllocator*& allocator : UploadCommandAllocators)
	{
		CHECK_RESULT(Native->CreateCommandAllocator(type, IID_PPV_ARGS(&allocator)));
	}
	CHECK_RESULT(Native->CreateCommandList1(0, type, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&UploadCommandList)));

	for (usize i = 0; i < FramesInFlight; ++i)
	{
		PendingDestroys.Add(Array<IUnknown*>(Allocator));
	}

	ConstantBufferShaderResourceUnorderedAccessViewHeap.Create(D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_1,
															   D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
															   true,
															   this);
	RenderTargetViewHeap.Create(D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_1, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, false, this);
	DepthStencilViewHeap.Create(D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_1, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, false, this);
	SamplerViewHeap.Create(D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, true, this);

	uint64 timestampFrequency;
	CHECK_RESULT(GraphicsQueue->GetTimestampFrequency(&timestampFrequency));
	TimestampFrequency = static_cast<double>(timestampFrequency);
}

Device::~Device()
{
	WaitForIdle();

	ReleaseAllDestroys();
	for (auto& [source, destination] : PendingUploads)
	{
		SAFE_RELEASE(source);
	}

	ConstantBufferShaderResourceUnorderedAccessViewHeap.Destroy();
	RenderTargetViewHeap.Destroy();
	DepthStencilViewHeap.Destroy();
	SamplerViewHeap.Destroy();

	SAFE_RELEASE(UploadCommandList);
	for (ID3D12CommandAllocator* commandAllocator : UploadCommandAllocators)
	{
		SAFE_RELEASE(commandAllocator);
	}
	SAFE_RELEASE(FrameFence);
	SAFE_RELEASE(SwapChain);
	SAFE_RELEASE(GraphicsQueue);
	SAFE_RELEASE(Native);

	Dxc::Shutdown();

#if DEBUG
	IDXGIDebug1* dxgiDebug = nullptr;
	CHECK_RESULT(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug)));
	CHECK_RESULT(dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, static_cast<DXGI_DEBUG_RLO_FLAGS>(DXGI_DEBUG_RLO_ALL | DXGI_DEBUG_RLO_IGNORE_INTERNAL)));
#endif
}

AccelerationStructure* Device::Create(const AccelerationStructureDescription& description)
{
	return Allocator->Create<AccelerationStructure>(description, this);
}

BufferView* Device::Create(const BufferViewDescription& description)
{
	return Allocator->Create<BufferView>(description, this);
}

ComputePipeline* Device::Create(const ComputePipelineDescription& description)
{
	return Allocator->Create<ComputePipeline>(description, this);
}

GraphicsContext* Device::Create(const GraphicsContextDescription& description)
{
	return Allocator->Create<GraphicsContext>(description, this);
}

GraphicsPipeline* Device::Create(const GraphicsPipelineDescription& description)
{
	return Allocator->Create<GraphicsPipeline>(description, this);
}

Resource* Device::Create(const ResourceDescription& description)
{
	return Allocator->Create<Resource>(description, this);
}

Sampler* Device::Create(const SamplerDescription& description)
{
	return Allocator->Create<Sampler>(description, this);
}

Shader* Device::Create(const ShaderDescription& description)
{
	return Allocator->Create<Shader>(description, this);
}

TextureView* Device::Create(const TextureViewDescription& description)
{
	return Allocator->Create<TextureView>(description, this);
}

void Device::Destroy(AccelerationStructure* accelerationStructure) const
{
	Allocator->Destroy(accelerationStructure);
}

void Device::Destroy(BufferView* bufferView) const
{
	Allocator->Destroy(bufferView);
}

void Device::Destroy(ComputePipeline* computePipeline) const
{
	Allocator->Destroy(computePipeline);
}

void Device::Destroy(GraphicsContext* graphicsContext) const
{
	Allocator->Destroy(graphicsContext);
}

void Device::Destroy(GraphicsPipeline* graphicsPipeline) const
{
	Allocator->Destroy(graphicsPipeline);
}

void Device::Destroy(Resource* resource) const
{
	Allocator->Destroy(resource);
}

void Device::Destroy(Sampler* sampler) const
{
	Allocator->Destroy(sampler);
}

void Device::Destroy(Shader* shader) const
{
	Allocator->Destroy(shader);
}

void Device::Destroy(TextureView* textureView) const
{
	Allocator->Destroy(textureView);
}

void Device::Write(Resource* resource, const void* data)
{
	resource->Write(data, &PendingUploads);
}

void Device::Submit(const GraphicsContext* context)
{
	ReleaseFrameDestroys();
	FlushUploads();

	context->Execute(GraphicsQueue);
}

void Device::Present()
{
	const uint64 frameFenceValue = FrameFenceValues[GetFrameIndex()];
	CHECK_RESULT(SwapChain->Present(1, 0));

	CHECK_RESULT(GraphicsQueue->Signal(FrameFence, frameFenceValue));

	if (FrameFence->GetCompletedValue() < FrameFenceValues[GetFrameIndex()])
	{
		CHECK_RESULT(FrameFence->SetEventOnCompletion(FrameFenceValues[GetFrameIndex()], nullptr));
		WaitForSingleObjectEx(nullptr, INFINITE, false);
	}
	FrameFenceValues[GetFrameIndex()] = frameFenceValue + 1;
}

void Device::WaitForIdle()
{
	const uint64 fenceValue = FrameFenceValues[GetFrameIndex()];
	CHECK_RESULT(GraphicsQueue->Signal(FrameFence, fenceValue));

	if (FrameFence->GetCompletedValue() < fenceValue)
	{
		CHECK_RESULT(FrameFence->SetEventOnCompletion(fenceValue, nullptr));
		WaitForSingleObjectEx(nullptr, INFINITE, false);
	}

	++FrameFenceValues[GetFrameIndex()];
}

void Device::ReleaseAllDestroys()
{
	for (Array<IUnknown*>& destroys : PendingDestroys)
	{
		for (IUnknown* resource : destroys)
		{
			SAFE_RELEASE(resource);
		}
		destroys.Clear();
	}
}

void Device::ResizeSwapChain(uint32 width, uint32 height)
{
	CHECK_RESULT(SwapChain->ResizeBuffers(FramesInFlight, width, height, DXGI_FORMAT_UNKNOWN, 0));

	for (uint64& frameFenceValue : FrameFenceValues)
	{
		frameFenceValue = GetFrameIndex();
	}

	RenderTargetViewHeap.Reset();
	DepthStencilViewHeap.Reset();
}

usize Device::GetFrameIndex() const
{
	return SwapChain->GetCurrentBackBufferIndex();
}

AccelerationStructureSize Device::GetAccelerationStructureSize(const AccelerationStructureGeometry& geometry) const
{
	const D3D12_RAYTRACING_GEOMETRY_DESC geometryDescription = To(geometry);
	const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = To(geometryDescription);

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO preBuildSizes;
	Native->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &preBuildSizes);

	return AccelerationStructureSize
	{
		.ResultSize = preBuildSizes.ResultDataMaxSizeInBytes,
		.ScratchSize = preBuildSizes.ScratchDataSizeInBytes,
	};
}

AccelerationStructureSize Device::GetAccelerationStructureSize(const Buffer& instances) const
{
	const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = To(instances);

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO preBuildSizes;
	Native->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &preBuildSizes);

	return AccelerationStructureSize
	{
		.ResultSize = preBuildSizes.ResultDataMaxSizeInBytes,
		.ScratchSize = preBuildSizes.ScratchDataSizeInBytes,
	};
}

usize Device::GetAccelerationStructureInstanceSize()
{
	return sizeof(D3D12_RAYTRACING_INSTANCE_DESC);
}

D3D12_CPU_DESCRIPTOR_HANDLE Device::GetCpu(usize index, ViewType type) const
{
	switch (type)
	{
	case ViewType::ConstantBuffer:
	case ViewType::ShaderResource:
	case ViewType::UnorderedAccess:
		return ConstantBufferShaderResourceUnorderedAccessViewHeap.GetCpu(index);
	case ViewType::Sampler:
		return SamplerViewHeap.GetCpu(index);
	case ViewType::RenderTarget:
		return RenderTargetViewHeap.GetCpu(index);
	case ViewType::DepthStencil:
		return DepthStencilViewHeap.GetCpu(index);
	}
	CHECK(false);
	return D3D12_CPU_DESCRIPTOR_HANDLE {};
}

D3D12_GPU_DESCRIPTOR_HANDLE Device::GetGpu(usize index, ViewType type) const
{
	switch (type)
	{
	case ViewType::ConstantBuffer:
	case ViewType::ShaderResource:
	case ViewType::UnorderedAccess:
		return ConstantBufferShaderResourceUnorderedAccessViewHeap.GetGpu(index);
	case ViewType::Sampler:
		return SamplerViewHeap.GetGpu(index);
	case ViewType::RenderTarget:
		return RenderTargetViewHeap.GetGpu(index);
	case ViewType::DepthStencil:
		return DepthStencilViewHeap.GetGpu(index);
	}
	CHECK(false);
	return D3D12_GPU_DESCRIPTOR_HANDLE {};
}

void Device::AddPendingDestroy(IUnknown* pendingDestroy)
{
	if (pendingDestroy)
	{
		PendingDestroys[GetFrameIndex()].Add(pendingDestroy);
	}
}

ID3D12Resource2* Device::GetSwapChainResource(usize backBufferIndex) const
{
	ID3D12Resource2* resource = nullptr;
	CHECK_RESULT(SwapChain->GetBuffer(static_cast<uint32>(backBufferIndex), IID_PPV_ARGS(&resource)));
	return resource;
}

void Device::FlushUploads()
{
	if (PendingUploads.IsEmpty())
	{
		return;
	}

	CHECK_RESULT(UploadCommandList->Reset(UploadCommandAllocators[GetFrameIndex()], nullptr));
	for (const auto& [source, destination] : PendingUploads)
	{
		destination->Upload(UploadCommandList, source);
		PendingDestroys[GetFrameIndex()].Add(source);
	}
	CHECK_RESULT(UploadCommandList->Close());

	ID3D12CommandList* upload[] = { UploadCommandList };
	GraphicsQueue->ExecuteCommandLists(ARRAY_COUNT(upload), upload);

	PendingUploads.Clear();
}

void Device::ReleaseFrameDestroys()
{
	for (IUnknown* resource : PendingDestroys[GetFrameIndex()])
	{
		SAFE_RELEASE(resource);
	}
	PendingDestroys[GetFrameIndex()].Clear();
}

}
