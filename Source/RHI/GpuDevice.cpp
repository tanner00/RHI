#include "GpuDevice.hpp"
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

struct RootParameter
{
	D3D12_ROOT_PARAMETER1 Parameter;
	Array<D3D12_DESCRIPTOR_RANGE1> Ranges;
};

namespace Dxc
{
static void Init();
static void Shutdown();

static Shader CompileShader(ShaderStage stage, StringView filePath);

static void ReflectInputElements(ID3D12ShaderReflection* shaderReflection, Array<D3D12_INPUT_ELEMENT_DESC>& inputElements);
static void ReflectRootParameters(ID3D12ShaderReflection* shaderReflection,
								  HashTable<String, RootParameter>& rootParameters);
}

static D3D12_FILTER ToD3D12(SamplerFilter filter)
{
	switch (filter)
	{
	case SamplerFilter::Point:
		return D3D12_FILTER_MIN_MAG_MIP_POINT;
	case SamplerFilter::Linear:
		return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	case SamplerFilter::Anisotropic:
		return D3D12_FILTER_ANISOTROPIC;
	}
	CHECK(false);
	return D3D12_FILTER_MIN_MAG_MIP_POINT;
}

static D3D12_TEXTURE_ADDRESS_MODE ToD3D12(SamplerAddress address)
{
	switch (address)
	{
	case SamplerAddress::Wrap:
		return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	case SamplerAddress::Mirror:
		return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
	case SamplerAddress::Clamp:
		return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	case SamplerAddress::Border:
		return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	}
	CHECK(false);
	return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
}

static DXGI_FORMAT ToD3D12View(TextureFormat format, DescriptorType type)
{
	switch (format)
	{
	case TextureFormat::Depth24Stencil8:
		if (type == DescriptorType::ShaderResource)
		{
			return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		}
		break;
	case TextureFormat::Depth32:
		if (type == DescriptorType::ShaderResource)
		{
			return DXGI_FORMAT_R32_TYPELESS;
		}
		break;
	}
	return ToD3D12(format);
}

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
	, Buffers(ResourceTableSize)
	, Textures(ResourceTableSize)
	, Samplers(ResourceTableSize)
	, Shaders(ResourceTableSize)
	, GraphicsPipelines(ResourceTableSize)
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
		DXGI_ADAPTER_DESC3 adapterDescriptor;
		CHECK_RESULT(adapter->GetDesc3(&adapterDescriptor));
		if (adapterDescriptor.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)
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

	constexpr D3D12_COMMAND_QUEUE_DESC graphicsQueueDescriptor =
	{
		.Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
		.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
		.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
		.NodeMask = 0,
	};
	CHECK_RESULT(Device->CreateCommandQueue(&graphicsQueueDescriptor, IID_PPV_ARGS(&GraphicsQueue)));

	const HWND windowHandle = static_cast<HWND>(window->Handle);
	constexpr DXGI_SWAP_CHAIN_DESC1 swapChainDescriptor =
	{
		.Width = 0,
		.Height = 0,
		.Format = SwapChainFormat,
		.Stereo = false,
		.SampleDesc = DefaultSampleDescriptor,
		.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_BACK_BUFFER,
		.BufferCount = FramesInFlight,
		.Scaling = DXGI_SCALING_STRETCH,
		.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
		.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED,
		.Flags = 0,
	};
	const constexpr DXGI_SWAP_CHAIN_FULLSCREEN_DESC* fullScreenSwapChainDescriptor = nullptr;
	IDXGISwapChain1* swapChain = nullptr;
	CHECK_RESULT(dxgiFactory->CreateSwapChainForHwnd(GraphicsQueue, windowHandle, &swapChainDescriptor, fullScreenSwapChainDescriptor, nullptr, &swapChain));
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

	constexpr D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	for (ID3D12CommandAllocator*& allocator : UploadCommandAllocators)
	{
		CHECK_RESULT(Device->CreateCommandAllocator(type, IID_PPV_ARGS(&allocator)));
	}
	CHECK_RESULT(Device->CreateCommandList1(0, type, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&UploadCommandList)));

	for (usize i = 0; i < FramesInFlight; ++i)
	{
		PendingDeletes.Add(Array<IUnknown*> {});
	}

	ConstantBufferShaderResourceUnorderedAccessDescriptorHeap.Create(D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_1,
																	 DescriptorHeapType::ConstantBufferShaderResourceUnorderedAccess, true, this);
	RenderTargetDescriptorHeap.Create(D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_1, DescriptorHeapType::RenderTarget, false, this);
	DepthStencilDescriptorHeap.Create(D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_1, DescriptorHeapType::DepthStencil, false, this);
	SamplerDescriptorHeap.Create(D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE, DescriptorHeapType::Sampler, true, this);

	UploadHeap.Create(MB(512), GpuHeapType::Upload, this);
	DefaultHeap.Create(MB(512), GpuHeapType::Default, this);
}

GpuDevice::~GpuDevice()
{
	WaitForIdle();
	ReleaseAllDeletes();

	UploadHeap.Destroy();
	DefaultHeap.Destroy();

	DepthStencilDescriptorHeap.Destroy();
	RenderTargetDescriptorHeap.Destroy();
	SamplerDescriptorHeap.Destroy();
	ConstantBufferShaderResourceUnorderedAccessDescriptorHeap.Destroy();

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

BufferHandle GpuDevice::CreateBuffer(StringView name, const BufferDescription& description)
{
	const BufferHandle handle = { HandleIndex++, description };
	Buffer buffer = {};

	CHECK(handle.GetSize() != 0);

	const usize resourceCount = handle.IsStream() ? FramesInFlight : 1;
	for (usize i = 0; i < resourceCount; ++i)
	{
		if (handle.IsStream())
		{
			buffer.Resources[i] = UploadHeap.AllocateBuffer(handle.GetSize(), name);
		}
		else
		{
			buffer.Resources[i] = DefaultHeap.AllocateBuffer(handle.GetSize(), name);
		}
	}

	Buffers.Add(handle.Get(), Move(buffer));
	return handle;
}

BufferHandle GpuDevice::CreateBuffer(StringView name, const void* staticData, const BufferDescription& description)
{
	const BufferHandle handle = CreateBuffer(name, description);
	CHECK(handle.IsStatic());

	BufferResource uploadResource = UploadHeap.AllocateBuffer(handle.GetSize(), "Upload [Buffer]"_view);
	PendingBufferUploads.Add({ uploadResource, handle });

	void* mapped = nullptr;
	CHECK_RESULT(uploadResource->Map(0, nullptr, &mapped));
	Platform::MemoryCopy(mapped, staticData, handle.GetSize());
	constexpr const D3D12_RANGE* writeEverything = nullptr;
	uploadResource->Unmap(0, writeEverything);

	return handle;
}

TextureHandle GpuDevice::CreateTexture(StringView name, BarrierLayout initialLayout, const TextureDescription& description, TextureResource existingResource)
{
	const TextureHandle handle = { HandleIndex++, description };
	Texture texture = {};

	if (existingResource)
	{
		texture.Resource = existingResource;
	}
	else
	{
		texture.Resource = DefaultHeap.AllocateTexture(handle, initialLayout, name);
	}
	Textures.Add(handle.Get(), Move(texture));
	return handle;
}

SamplerHandle GpuDevice::CreateSampler(const SamplerDescription& description)
{
	const SamplerHandle handle = { HandleIndex++, description };
	Sampler sampler = {};

	const usize heapIndex = SamplerDescriptorHeap.AllocateIndex();
	sampler.HeapIndex = heapIndex;

	const D3D12_SAMPLER_DESC2 samplerDescriptor =
	{
		.Filter = ToD3D12(description.Filter),
		.AddressU = ToD3D12(description.Address),
		.AddressV = ToD3D12(description.Address),
		.AddressW = ToD3D12(description.Address),
		.MipLODBias = 0,
		.MaxAnisotropy = D3D12_DEFAULT_MAX_ANISOTROPY,
		.ComparisonFunc = D3D12_COMPARISON_FUNC_NONE,
		.FloatBorderColor =
		{
			description.BorderFilterColor.X,
			description.BorderFilterColor.Y,
			description.BorderFilterColor.Z,
			description.BorderFilterColor.W,
		},
		.MinLOD = 0.0f,
		.MaxLOD = D3D12_FLOAT32_MAX,
		.Flags = D3D12_SAMPLER_FLAG_NONE,
	};
	Device->CreateSampler2(&samplerDescriptor, D3D12_CPU_DESCRIPTOR_HANDLE {SamplerDescriptorHeap.GetCpuDescriptor(heapIndex) });

	Samplers.Add(handle.Get(), Move(sampler));
	return handle;
}

ShaderHandle GpuDevice::CreateShader(const ShaderDescription& description)
{
	const ShaderHandle handle = { HandleIndex++, description };
	Shader result = Dxc::CompileShader(handle.GetStage(), handle.GetFilePath());

	Shaders.Add(handle.Get(), Move(result));
	return handle;
}

GraphicsPipelineHandle GpuDevice::CreateGraphicsPipeline(StringView name, const GraphicsPipelineDescription& description)
{
	static constexpr usize bindingBucketCount = 4;

	const GraphicsPipelineHandle handle = { HandleIndex++, description };

	HashTable<String, usize> parameters(bindingBucketCount);
	ID3D12RootSignature* rootSignature = nullptr;
	ID3D12PipelineState* pipelineState = nullptr;

	CHECK(handle.HasShaderStage(ShaderStage::Vertex));
	const bool usesPixelShader = handle.HasShaderStage(ShaderStage::Pixel);
	if (usesPixelShader)
	{
		CHECK(handle.GetStageCount() == 2);
	}
	else
	{
		CHECK(handle.GetStageCount() == 1);
	}

	const Shader* vertex = &Shaders[handle.GetShaderStage(ShaderStage::Vertex).Get()];
	const Shader* pixel = usesPixelShader ? &Shaders[handle.GetShaderStage(ShaderStage::Pixel).Get()] : nullptr;

	Array<D3D12_INPUT_ELEMENT_DESC> inputElements;
	Dxc::ReflectInputElements(vertex->Reflection, inputElements);

	HashTable<String, RootParameter> rootParameters(bindingBucketCount);
	Dxc::ReflectRootParameters(vertex->Reflection, rootParameters);
	if (usesPixelShader)
	{
		Dxc::ReflectRootParameters(pixel->Reflection, rootParameters);
	}

	Array<D3D12_ROOT_PARAMETER1> rootParametersList(rootParameters.GetCount());
	usize rootParameterIndex = 0;
	for (auto& [resourceName, rootParameter] : rootParameters)
	{
		rootParametersList.Add(rootParameter.Parameter);

		parameters.Add(Move(resourceName), rootParameterIndex);
		++rootParameterIndex;
	}

	const D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescriptor =
	{
		.Version = D3D_ROOT_SIGNATURE_VERSION_1_1,
		.Desc_1_1 =
		{
			.NumParameters = static_cast<uint32>(rootParametersList.GetLength()),
			.pParameters = rootParametersList.GetData(),
			.NumStaticSamplers = 0,
			.pStaticSamplers = nullptr,
			.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT,
		},
	};
	ID3DBlob* serializedRootSignature = nullptr;
	ID3DBlob* errorBlob = nullptr;
	const HRESULT rootSignatureResult = D3D12SerializeVersionedRootSignature(&rootSignatureDescriptor, &serializedRootSignature, &errorBlob);
#if DEBUG
	if (FAILED(rootSignatureResult) && errorBlob)
	{
		char errorMessage[512] = {};
		Platform::StringPrint("Root Signature Error: %s\n", errorMessage, sizeof(errorMessage), errorBlob->GetBufferPointer());
		Platform::FatalError(errorMessage);
	}
#else
	(void)rootSignatureResult;
#endif
	CHECK(serializedRootSignature);
	CHECK_RESULT(Device->CreateRootSignature(0, serializedRootSignature->GetBufferPointer(), serializedRootSignature->GetBufferSize(),
											 IID_PPV_ARGS(&rootSignature)));
#if DEBUG
	CHECK_RESULT(rootSignature->SetPrivateData(D3DDebugObjectName, static_cast<uint32>(name.Length), name.Buffer));
#else
	(void)name;
#endif

	constexpr D3D12_RENDER_TARGET_BLEND_DESC defaultBlendDescriptor =
	{
		.BlendEnable = false,
		.LogicOpEnable = false,
		.SrcBlend = D3D12_BLEND_ONE,
		.DestBlend = D3D12_BLEND_ZERO,
		.BlendOp = D3D12_BLEND_OP_ADD,
		.SrcBlendAlpha = D3D12_BLEND_ONE,
		.DestBlendAlpha = D3D12_BLEND_ZERO,
		.BlendOpAlpha = D3D12_BLEND_OP_ADD,
		.LogicOp = D3D12_LOGIC_OP_NOOP,
		.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL,
	};
	const D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDescriptor =
	{
		.pRootSignature = rootSignature,
		.VS =
		{
			.pShaderBytecode = vertex->Blob->GetBufferPointer(),
			.BytecodeLength = vertex->Blob->GetBufferSize(),
		},
		.PS =
		{
			.pShaderBytecode = usesPixelShader ? pixel->Blob->GetBufferPointer() : nullptr,
			.BytecodeLength = usesPixelShader ? pixel->Blob->GetBufferSize() : 0,
		},
		.StreamOutput = {},
		.BlendState =
		{
			.AlphaToCoverageEnable = false,
			.IndependentBlendEnable = false,
			.RenderTarget =
			{
				defaultBlendDescriptor,
				defaultBlendDescriptor,
				defaultBlendDescriptor,
				defaultBlendDescriptor,
				defaultBlendDescriptor,
				defaultBlendDescriptor,
				defaultBlendDescriptor,
				defaultBlendDescriptor,
			},
		},
		.SampleMask = D3D12_DEFAULT_SAMPLE_MASK,
		.RasterizerState =
		{
			.FillMode = D3D12_FILL_MODE_SOLID,
			.CullMode = D3D12_CULL_MODE_BACK,
			.FrontCounterClockwise = true,
			.DepthBias = D3D12_DEFAULT_DEPTH_BIAS,
			.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP,
			.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
			.DepthClipEnable = true,
			.MultisampleEnable = false,
			.AntialiasedLineEnable = false,
			.ForcedSampleCount = 0,
			.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF,
		},
		.DepthStencilState =
		{
			.DepthEnable = IsDepthFormat(handle.GetDepthFormat()),
			.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL,
			.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL,
			.StencilEnable = IsStencilFormat(handle.GetDepthFormat()),
			.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK,
			.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK,
			.FrontFace =
			{
				.StencilFailOp = D3D12_STENCIL_OP_KEEP,
				.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP,
				.StencilPassOp = D3D12_STENCIL_OP_KEEP,
				.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS
			},
			.BackFace =
			{
				.StencilFailOp = D3D12_STENCIL_OP_KEEP,
				.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP,
				.StencilPassOp = D3D12_STENCIL_OP_KEEP,
				.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS
			},
		},
		.InputLayout =
		{
			.pInputElementDescs = inputElements.GetData(),
			.NumElements = static_cast<uint32>(inputElements.GetLength()),
		},
		.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED,
		.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		.NumRenderTargets = usesPixelShader ? 1U : 0U,
		.RTVFormats =
		{
			ToD3D12(handle.GetRenderTargetFormat()),
		},
		.DSVFormat = ToD3D12(handle.GetDepthFormat()),
		.SampleDesc = DefaultSampleDescriptor,
		.NodeMask = 0,
		.CachedPSO = {},
		.Flags = D3D12_PIPELINE_STATE_FLAG_NONE,
	};
	CHECK_RESULT(Device->CreateGraphicsPipelineState(&graphicsPipelineStateDescriptor, IID_PPV_ARGS(&pipelineState)));
#if DEBUG
	CHECK_RESULT(pipelineState->SetPrivateData(D3DDebugObjectName, static_cast<uint32>(name.Length), name.Buffer));
#else
	(void)name;
#endif
	GraphicsPipeline graphicsPipeline =
	{
		.Parameters = Move(parameters),
		.RootSignature = rootSignature,
		.PipelineState = pipelineState,
	};
	GraphicsPipelines.Add(handle.Get(), Move(graphicsPipeline));
	return handle;
}

void GpuDevice::DestroyBuffer(BufferHandle& handle)
{
	if (!handle.IsValid())
	{
		return;
	}

	for (const BufferResource resource : Buffers[handle.Get()].Resources)
	{
		AddPendingDelete(resource);
	}
	Buffers.Remove(handle.Get());

	handle.Reset();
}

void GpuDevice::DestroyTexture(TextureHandle& handle)
{
	if (!handle.IsValid())
	{
		return;
	}

	const Texture& texture = Textures[handle.Get()];
	AddPendingDelete(texture.Resource);
	Textures.Remove(handle.Get());

	handle.Reset();
}

void GpuDevice::DestroySampler(SamplerHandle& handle)
{
	if (!handle.IsValid())
	{
		return;
	}

	Samplers.Remove(handle.Get());

	handle.Reset();
}

void GpuDevice::DestroyShader(ShaderHandle& handle)
{
	if (!handle.IsValid())
	{
		return;
	}

	const Shader& shader = Shaders[handle.Get()];
	AddPendingDelete(shader.Blob);
	AddPendingDelete(shader.Reflection);
	Shaders.Remove(handle.Get());

	handle.Reset();
}

void GpuDevice::DestroyGraphicsPipeline(GraphicsPipelineHandle& handle)
{
	if (!handle.IsValid())
	{
		return;
	}

	const GraphicsPipeline& pipeline = GraphicsPipelines[handle.Get()];
	AddPendingDelete(pipeline.PipelineState);
	AddPendingDelete(pipeline.RootSignature);
	GraphicsPipelines.Remove(handle.Get());
	handle.Reset();
}

GraphicsContext GpuDevice::CreateGraphicsContext()
{
	return GraphicsContext(this);
}

void GpuDevice::Write(const BufferHandle& handle, const void* data)
{
	const Buffer& buffer = Buffers[handle.Get()];

	BufferResource resource = nullptr;
	CHECK(!handle.IsStatic());
	if (handle.IsStream())
	{
		resource = buffer.GetBufferResource(GetFrameIndex(), true);
	}
	else if (handle.IsDynamic())
	{
		resource = UploadHeap.AllocateBuffer(handle.GetSize(), "Upload [Buffer]"_view);
		PendingBufferUploads.Add({ resource, handle });
	}

	void* mapped = nullptr;
	CHECK_RESULT(resource->Map(0, nullptr, &mapped));
	Platform::MemoryCopy(mapped, data, handle.GetSize());
	constexpr const D3D12_RANGE* writeEverything = nullptr;
	resource->Unmap(0, writeEverything);
}

void GpuDevice::Write(const TextureHandle& handle, const void* data)
{
	CHECK(handle.GetType() == TextureType::Rectangle);

	const D3D12_RESOURCE_DESC1 textureDescriptor =
	{
		.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
		.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
		.Width = handle.GetWidth(),
		.Height = handle.GetHeight(),
		.DepthOrArraySize = static_cast<uint16>(handle.GetCount()),
		.MipLevels = 1,
		.Format = ToD3D12(handle.GetFormat()),
		.SampleDesc = DefaultSampleDescriptor,
		.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
		.Flags = D3D12_RESOURCE_FLAG_NONE,
		.SamplerFeedbackMipRegion = {},
	};
	constexpr uint32 singleResource = 1;
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
	uint32 rowCount;
	uint64 rowSize;
	uint64 totalSize;
	Device->GetCopyableFootprints1(&textureDescriptor, 0, singleResource, 0, &layout, &rowCount, &rowSize, &totalSize);

	const BufferResource resource = UploadHeap.AllocateBuffer(totalSize, "Upload [Texture]"_view);

	void* mapped = nullptr;
	CHECK_RESULT(resource->Map(0, nullptr, &mapped));
	for (usize row = 0; row < rowCount; ++row)
	{
		Platform::MemoryCopy
		(
			static_cast<uint8*>(mapped) + row * layout.Footprint.RowPitch,
			static_cast<const uint8*>(data) + row * rowSize,
			rowSize
		);
	}
	constexpr const D3D12_RANGE* writeEverything = nullptr;
	resource->Unmap(0, writeEverything);

	PendingTextureUploads.Add({ resource, handle });
}

void GpuDevice::WriteCubemap(const TextureHandle& handle, const Array<uint8*>& faces)
{
	CHECK(handle.GetType() == TextureType::Cubemap);
	CHECK(faces.GetLength() == 6);

	usize totalSize = 0;
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT layouts[6];
	usize subresourceSizes[6];
	uint32 rowCounts[6];
	usize rowSizes[6];

	for (usize subresourceIndex = 0; subresourceIndex < handle.GetCount(); ++subresourceIndex)
	{
		const D3D12_RESOURCE_DESC1 textureDescriptor =
		{
			.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
			.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
			.Width = handle.GetWidth(),
			.Height = handle.GetHeight(),
			.DepthOrArraySize = static_cast<uint16>(handle.GetCount()),
			.MipLevels = 1,
			.Format = ToD3D12(handle.GetFormat()),
			.SampleDesc = DefaultSampleDescriptor,
			.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
			.Flags = D3D12_RESOURCE_FLAG_NONE,
			.SamplerFeedbackMipRegion = {},
		};
		constexpr uint32 singleResource = 1;
		Device->GetCopyableFootprints1(&textureDescriptor, static_cast<uint32>(subresourceIndex), singleResource, 0,
									   &layouts[subresourceIndex], &rowCounts[subresourceIndex], &rowSizes[subresourceIndex], &subresourceSizes[subresourceIndex]);

		totalSize += subresourceSizes[subresourceIndex];
	}
	const BufferResource resource = UploadHeap.AllocateBuffer(totalSize, "Upload [Cubemap Texture]"_view);

	void* mapped = nullptr;
	CHECK_RESULT(resource->Map(0, nullptr, &mapped));

	for (usize subresourceIndex = 0; subresourceIndex < handle.GetCount(); ++subresourceIndex)
	{
		for (usize row = 0; row < rowCounts[subresourceIndex]; ++row)
		{
			Platform::MemoryCopy
			(
				static_cast<uint8*>(mapped) + subresourceIndex * subresourceSizes[subresourceIndex] + row * layouts[subresourceIndex].Footprint.RowPitch,
				static_cast<const uint8*>(faces[subresourceIndex]) + row * rowSizes[subresourceIndex],
				rowSizes[subresourceIndex]
			);
		}
	}

	constexpr const D3D12_RANGE* writeEverything = nullptr;
	resource->Unmap(0, writeEverything);

	PendingTextureUploads.Add({ resource, handle });
}

void GpuDevice::Submit(const GraphicsContext& context)
{
	ReleaseFrameDeletes();

	if (!PendingBufferUploads.IsEmpty() || !PendingTextureUploads.IsEmpty())
	{
		CHECK_RESULT(UploadCommandList->Reset(UploadCommandAllocators[FrameIndex], nullptr));

		for (const auto& [source, destination] : PendingTextureUploads)
		{
			const Texture& destinationTexture = Textures[destination.Get()];

			for (usize subresourceIndex = 0; subresourceIndex < destination.GetCount(); ++subresourceIndex)
			{
				const D3D12_RESOURCE_DESC1 textureDescriptor =
				{
					.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
					.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
					.Width = destination.GetWidth(),
					.Height = destination.GetHeight(),
					.DepthOrArraySize = static_cast<uint16>(destination.GetCount()),
					.MipLevels = 1,
					.Format = ToD3D12(destination.GetFormat()),
					.SampleDesc = DefaultSampleDescriptor,
					.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
					.Flags = D3D12_RESOURCE_FLAG_NONE,
					.SamplerFeedbackMipRegion = {},
				};
				constexpr uint32 singleResource = 1;
				D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
				uint32 rowCount;
				Device->GetCopyableFootprints1(&textureDescriptor, static_cast<uint32>(subresourceIndex), singleResource, 0,
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
			const Buffer& destinationBuffer = Buffers[destination.Get()];
			UploadCommandList->CopyResource(destinationBuffer.GetOnlyBufferResource(), source);

			PendingDeletes[FrameIndex].Add(source);
		}

		CHECK_RESULT(UploadCommandList->Close());

		ID3D12CommandList* upload[] = { UploadCommandList };
		GraphicsQueue->ExecuteCommandLists(ARRAY_COUNT(upload), upload);

		PendingBufferUploads.Clear();
		PendingTextureUploads.Clear();
	}

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

	RenderTargetDescriptorHeap.Reset();
	DepthStencilDescriptorHeap.Reset();
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

void GpuDevice::EnsureConstantBufferDescriptor(const BufferHandle& handle)
{
	constexpr DescriptorType descriptorType = DescriptorType::ConstantBuffer;

	Buffer& buffer = Buffers[handle.Get()];
	if (buffer.HeapIndices[0][static_cast<usize>(descriptorType)])
	{
		return;
	}

	CHECK((handle.GetSize() % D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT) == 0);
	CHECK((handle.GetStride() % D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT) == 0);

	for (usize i = 0; i < ARRAY_COUNT(buffer.Resources); ++i)
	{
		if (buffer.Resources[i])
		{
			const usize heapIndex = ConstantBufferShaderResourceUnorderedAccessDescriptorHeap.AllocateIndex();
			buffer.HeapIndices[i][static_cast<usize>(descriptorType)] = heapIndex;

			const D3D12_CONSTANT_BUFFER_VIEW_DESC constantBufferViewDescriptor =
			{
				.BufferLocation = buffer.Resources[i]->GetGPUVirtualAddress(),
				.SizeInBytes = static_cast<uint32>(handle.GetSize()),
			};
			Device->CreateConstantBufferView
			(
				&constantBufferViewDescriptor,
				D3D12_CPU_DESCRIPTOR_HANDLE { ConstantBufferShaderResourceUnorderedAccessDescriptorHeap.GetCpuDescriptor(heapIndex) }
			);
		}
	}
}

void GpuDevice::EnsureShaderResourceDescriptor(const TextureHandle& handle)
{
	constexpr DescriptorType descriptorType = DescriptorType::ShaderResource;

	Texture& texture = Textures[handle.Get()];
	if (texture.HeapIndices[static_cast<usize>(descriptorType)])
	{
		return;
	}

	const usize heapIndex = ConstantBufferShaderResourceUnorderedAccessDescriptorHeap.AllocateIndex();
	texture.HeapIndices[static_cast<usize>(descriptorType)] = heapIndex;

	D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDescriptor;
	switch (handle.GetType())
	{
	case TextureType::Rectangle:
		shaderResourceViewDescriptor =
		{
			.Format = ToD3D12View(handle.GetFormat(), descriptorType),
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
		shaderResourceViewDescriptor =
		{
			.Format = ToD3D12View(handle.GetFormat(), descriptorType),
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
		texture.Resource,
		&shaderResourceViewDescriptor,
		D3D12_CPU_DESCRIPTOR_HANDLE { ConstantBufferShaderResourceUnorderedAccessDescriptorHeap.GetCpuDescriptor(heapIndex) }
	);
}

void GpuDevice::EnsureRenderTargetDescriptor(const TextureHandle& handle)
{
	constexpr DescriptorType descriptorType = DescriptorType::RenderTarget;

	Texture& texture = Textures[handle.Get()];
	if (texture.HeapIndices[static_cast<usize>(descriptorType)])
	{
		return;
	}

	const usize heapIndex = RenderTargetDescriptorHeap.AllocateIndex();
	texture.HeapIndices[static_cast<usize>(descriptorType)] = heapIndex;

	const D3D12_RENDER_TARGET_VIEW_DESC renderTargetViewDescriptor =
	{
		.Format = ToD3D12View(handle.GetFormat(), descriptorType),
		.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D,
		.Texture2D =
		{
			.MipSlice = 0,
			.PlaneSlice = 0,
		},
	};
	Device->CreateRenderTargetView(
		texture.Resource,
		&renderTargetViewDescriptor,
		D3D12_CPU_DESCRIPTOR_HANDLE { RenderTargetDescriptorHeap.GetCpuDescriptor(heapIndex) }
	);
}

void GpuDevice::EnsureDepthStencilDescriptor(const TextureHandle& handle)
{
	constexpr DescriptorType descriptorType = DescriptorType::DepthStencil;

	Texture& texture = Textures[handle.Get()];
	if (texture.HeapIndices[static_cast<usize>(descriptorType)])
	{
		return;
	}

	const usize heapIndex = DepthStencilDescriptorHeap.AllocateIndex();
	texture.HeapIndices[static_cast<usize>(descriptorType)] = heapIndex;

	const D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilViewDescriptor =
	{
		.Format = ToD3D12View(handle.GetFormat(), descriptorType),
		.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
		.Texture2D =
		{
			.MipSlice = 0,
		},
	};
	Device->CreateDepthStencilView
	(
		texture.Resource,
		&depthStencilViewDescriptor,
		D3D12_CPU_DESCRIPTOR_HANDLE { DepthStencilDescriptorHeap.GetCpuDescriptor(heapIndex) }
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

namespace Dxc
{

static IDxcCompiler3* Compiler = nullptr;
static IDxcUtils* Utils = nullptr;

static void Init()
{
	constexpr IMalloc* dxcAllocator = nullptr;
	HRESULT result = DxcCreateInstance2(dxcAllocator, CLSID_DxcCompiler, IID_PPV_ARGS(&Compiler));
	CHECK(SUCCEEDED(result));
	result = DxcCreateInstance2(dxcAllocator, CLSID_DxcUtils, IID_PPV_ARGS(&Utils));
	CHECK(SUCCEEDED(result));
}

static void Shutdown()
{
	SAFE_RELEASE(Utils);
	SAFE_RELEASE(Compiler);
}

Shader CompileShader(ShaderStage stage, StringView filePath)
{
	CHECK(Compiler && Utils);

	wchar_t pathBuffer[MAX_PATH];
	VERIFY(filePath.Length < sizeof(pathBuffer), "File path length limit exceeded!");
	const errno_t error = mbstowcs_s(nullptr, pathBuffer, reinterpret_cast<const char*>(filePath.Buffer), filePath.Length);
	CHECK(error == 0);

	uint32 codePage = DXC_CP_UTF8;
	IDxcBlobEncoding* sourceBlob = nullptr;
	HRESULT dxcResult = Utils->LoadFile(pathBuffer, &codePage, &sourceBlob);
	VERIFY(SUCCEEDED(dxcResult), "Failed to read shader from filesystem!");

	const wchar_t* arguments[] =
	{
		DXC_ARG_WARNINGS_ARE_ERRORS,
		L"-HV", L"2021",
		L"-all_resources_bound",
#if DEBUG
		DXC_ARG_OPTIMIZATION_LEVEL0,
		DXC_ARG_SKIP_OPTIMIZATIONS,
#endif
#if RELEASE
		L"-Qstrip_debug",
#endif
#if DEBUG || PROFILE
		DXC_ARG_DEBUG,
		L"-Qembed_debug",
#endif
#if RELEASE || PROFILE
		DXC_ARG_OPTIMIZATION_LEVEL3,
#endif
	};

	LPCWSTR entryPoint = nullptr;
	LPCWSTR profile = nullptr;
	switch (stage)
	{
	case ShaderStage::Vertex:
		entryPoint = L"VertexMain";
		profile = L"vs_6_0";
		break;
	case ShaderStage::Pixel:
		entryPoint = L"PixelMain";
		profile = L"ps_6_0";
		break;
	default:
		CHECK(false);
	}
	constexpr const DxcDefine* defines = nullptr;

	IDxcCompilerArgs* compileArguments = nullptr;
	CHECK_RESULT(Utils->BuildArguments(pathBuffer, entryPoint, profile, arguments, ARRAY_COUNT(arguments), defines, 0, &compileArguments));

	const DxcBuffer buffer =
	{
		.Ptr = static_cast<uint8*>(sourceBlob->GetBufferPointer()),
		.Size = sourceBlob->GetBufferSize(),
		.Encoding = DXC_CP_UTF8,
	};
	IDxcResult* compileResult = nullptr;
	dxcResult = Compiler->Compile(&buffer, compileArguments->GetArguments(), compileArguments->GetCount(), nullptr, IID_PPV_ARGS(&compileResult));
	CHECK(SUCCEEDED(dxcResult) && compileResult);

	SAFE_RELEASE(compileArguments);
	SAFE_RELEASE(sourceBlob);

	const HRESULT statusResult = compileResult->GetStatus(&dxcResult);
	CHECK(SUCCEEDED(statusResult));
#if DEBUG
	if (FAILED(dxcResult))
	{
		IDxcBlobEncoding* dxcErrorBlob = nullptr;
		dxcResult = compileResult->GetErrorBuffer(&dxcErrorBlob);
		CHECK(SUCCEEDED(dxcResult) && dxcErrorBlob);
		char errorMessage[2048];
		Platform::StringPrint("Shader Compiler: %s", errorMessage, sizeof(errorMessage), dxcErrorBlob->GetBufferPointer());
		Platform::FatalError(errorMessage);
	}
#endif

	IDxcBlob* shaderBlob = nullptr;
	CHECK_RESULT(compileResult->GetResult(&shaderBlob));

	IDxcBlob* reflectionBlob = {};
	CHECK_RESULT(compileResult->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(&reflectionBlob), nullptr));

	const DxcBuffer reflectionBuffer =
	{
		.Ptr = reflectionBlob->GetBufferPointer(),
		.Size = reflectionBlob->GetBufferSize(),
		.Encoding = 0,
	};
	ID3D12ShaderReflection* shaderReflection = {};
	CHECK_RESULT(Utils->CreateReflection(&reflectionBuffer, IID_PPV_ARGS(&shaderReflection)));

	SAFE_RELEASE(reflectionBlob);
	SAFE_RELEASE(compileResult);

	return Shader { shaderBlob, shaderReflection };
}

static DXGI_FORMAT MaskToFormat(uint8 mask)
{
	switch (mask)
	{
	case 0b1:
		return DXGI_FORMAT_R32_FLOAT;
	case 0b11:
		return DXGI_FORMAT_R32G32_FLOAT;
	case 0b111:
		return DXGI_FORMAT_R32G32B32_FLOAT;
	case 0b1111:
		return DXGI_FORMAT_R32G32B32A32_FLOAT;
	default:
		break;
	}
	CHECK(false);
	return DXGI_FORMAT_UNKNOWN;
}

static void ReflectInputElements(ID3D12ShaderReflection* shaderReflection, Array<D3D12_INPUT_ELEMENT_DESC>& inputElements)
{
	D3D12_SHADER_DESC shaderDescriptor = {};
	CHECK_RESULT(shaderReflection->GetDesc(&shaderDescriptor));

	for (uint32 i = 0; i < shaderDescriptor.InputParameters; ++i)
	{
		D3D12_SIGNATURE_PARAMETER_DESC inputParameterDescriptor = {};
		CHECK_RESULT(shaderReflection->GetInputParameterDesc(i, &inputParameterDescriptor));

		inputElements.Add
		(
			D3D12_INPUT_ELEMENT_DESC
			{
				.SemanticName = inputParameterDescriptor.SemanticName,
				.SemanticIndex = inputParameterDescriptor.SemanticIndex,
				.Format = MaskToFormat(inputParameterDescriptor.Mask),
				.InputSlot = inputParameterDescriptor.Register,
				.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT,
				.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
				.InstanceDataStepRate = 0,
			}
		);
	}
}

static void ReflectRootParameters(ID3D12ShaderReflection* shaderReflection,
								  HashTable<String, RootParameter>& rootParameters)
{
	D3D12_SHADER_DESC shaderDescriptor = {};
	CHECK_RESULT(shaderReflection->GetDesc(&shaderDescriptor));

	for (uint32 i = 0; i < shaderDescriptor.BoundResources; ++i)
	{
		D3D12_SHADER_INPUT_BIND_DESC resourceDescriptor = {};
		CHECK_RESULT(shaderReflection->GetResourceBindingDesc(i, &resourceDescriptor));

		String resourceName = { reinterpret_cast<const uint8*>(resourceDescriptor.Name), Platform::StringLength(resourceDescriptor.Name) };

		RootParameter parameter;

		if (resourceDescriptor.Type == D3D_SIT_CBUFFER)
		{
			parameter.Parameter = D3D12_ROOT_PARAMETER1
			{
				.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV,
				.Descriptor =
				{
					.ShaderRegister = resourceDescriptor.BindPoint,
					.RegisterSpace = resourceDescriptor.Space,
					.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE,
				},
				.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
			};
		}
		else if (resourceDescriptor.Type == D3D_SIT_TEXTURE)
		{
			parameter.Ranges.Add(
			{
				.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
				.NumDescriptors = resourceDescriptor.BindCount,
				.BaseShaderRegister = resourceDescriptor.BindPoint,
				.RegisterSpace = resourceDescriptor.Space,
				.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE,
				.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND,
			});

			parameter.Parameter = D3D12_ROOT_PARAMETER1
			{
				.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
				.DescriptorTable =
				{
					.NumDescriptorRanges = static_cast<uint32>(parameter.Ranges.GetLength()),
					.pDescriptorRanges = parameter.Ranges.GetData(),
				},
				.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL,
			};
		}
		else if (resourceDescriptor.Type == D3D_SIT_SAMPLER)
		{
			parameter.Ranges.Add(
			{
				.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,
				.NumDescriptors = resourceDescriptor.BindCount,
				.BaseShaderRegister = resourceDescriptor.BindPoint,
				.RegisterSpace = resourceDescriptor.Space,
				.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE,
				.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND,
			});

			parameter.Parameter = D3D12_ROOT_PARAMETER1
			{
				.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
				.DescriptorTable =
				{
					.NumDescriptorRanges = static_cast<uint32>(parameter.Ranges.GetLength()),
					.pDescriptorRanges = parameter.Ranges.GetData(),
				},
				.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL,
			};
		}
		else
		{
			CHECK(false);
		}

		rootParameters.Add
		(
			Move(resourceName),
			Move(parameter)
		);
	}
}

}
