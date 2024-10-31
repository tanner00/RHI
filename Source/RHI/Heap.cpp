#include "Heap.hpp"
#include "BarrierConversion.hpp"
#include "GpuDevice.hpp"

#include "Luft/Math.hpp"

#include "D3D12/d3d12.h"

static D3D12_HEAP_TYPE ToD3D12(GpuHeapType type)
{
	switch (type)
	{
	case GpuHeapType::Default:
		return D3D12_HEAP_TYPE_DEFAULT;
	case GpuHeapType::Upload:
		return D3D12_HEAP_TYPE_UPLOAD;
	}
	CHECK(false);
	return D3D12_HEAP_TYPE_DEFAULT;
}

static D3D12_DESCRIPTOR_HEAP_TYPE ToD3D12(ViewHeapType type)
{
	switch (type)
	{
	case ViewHeapType::ConstantBufferShaderResourceUnorderedAccess:
		return D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	case ViewHeapType::Sampler:
		return D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
	case ViewHeapType::RenderTarget:
		return D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	case ViewHeapType::DepthStencil:
		return D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	}
	CHECK(false);
	return D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
}

void GpuHeap::Create(usize size, GpuHeapType type, GpuDevice* device)
{
	Size = size;
	Device = device;
	Offset = 0;

	const D3D12_HEAP_DESC heapDescription =
	{
		.SizeInBytes = size,
		.Properties =
		{
			.Type = ToD3D12(type),
			.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
			.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
			.CreationNodeMask = 0,
			.VisibleNodeMask = 0,
		},
		.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
		.Flags = D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES | D3D12_HEAP_FLAG_CREATE_NOT_ZEROED,
	};
	CHECK_RESULT(device->GetDevice()->CreateHeap(&heapDescription, IID_PPV_ARGS(&Heap)));
}

void GpuHeap::Destroy()
{
	SAFE_RELEASE(Heap);
	Size = 0;
	Device = nullptr;
}

BufferResource GpuHeap::AllocateBuffer(usize size, StringView name)
{
	const D3D12_RESOURCE_DESC1 bufferDescription =
	{
		.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
		.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
		.Width = size,
		.Height = 1,
		.DepthOrArraySize = 1,
		.MipLevels = 1,
		.Format = DXGI_FORMAT_UNKNOWN,
		.SampleDesc = DefaultSampleDescription,
		.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
		.Flags = D3D12_RESOURCE_FLAG_NONE,
		.SamplerFeedbackMipRegion = {},
	};

	const usize newOffset = NextMultipleOf(Offset + size, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
	CHECK(newOffset <= Size);

	static constexpr const DXGI_FORMAT* noCastableFormats = nullptr;
	BufferResource resource = nullptr;
	CHECK_RESULT(Device->GetDevice()->CreatePlacedResource2(Heap, Offset, &bufferDescription, ToD3D12(BarrierLayout::Undefined),
															nullptr, 0, noCastableFormats, IID_PPV_ARGS(&resource)));
	Offset = newOffset;

#if DEBUG
	CHECK_RESULT(resource->SetPrivateData(D3DDebugObjectName, static_cast<uint32>(name.GetLength()), name.GetData()));
#else
	(void)name;
#endif
	return resource;
}

TextureResource GpuHeap::AllocateTexture(const TextureHandle& handle, BarrierLayout initialLayout, StringView name)
{
	const D3D12_RESOURCE_FLAGS renderTargetFlag = handle.IsRenderTarget() ? D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET : D3D12_RESOURCE_FLAG_NONE;
	const D3D12_RESOURCE_FLAGS depthStencilFlag = IsDepthFormat(handle.GetFormat()) ? D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL : D3D12_RESOURCE_FLAG_NONE;
	CHECK((renderTargetFlag & depthStencilFlag) == 0);

	const D3D12_RESOURCE_DESC1 textureDescription =
	{
		.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
		.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
		.Width = handle.GetWidth(),
		.Height = handle.GetHeight(),
		.DepthOrArraySize = static_cast<uint16>(handle.GetCount()),
		.MipLevels = 1,
		.Format = ToD3D12(handle.GetFormat()),
		.SampleDesc = DefaultSampleDescription,
		.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
		.Flags = renderTargetFlag | depthStencilFlag,
		.SamplerFeedbackMipRegion = {},
	};
	const D3D12_CLEAR_VALUE depthClear =
	{
		.Format = ToD3D12(handle.GetFormat()),
		.DepthStencil =
		{
			.Depth = D3D12_MAX_DEPTH,
			.Stencil = 0,
		},
	};

	const D3D12_RESOURCE_ALLOCATION_INFO allocationInfo = Device->GetDevice()->GetResourceAllocationInfo2(0, 1, &textureDescription, nullptr);
	const usize newOffset = NextMultipleOf(Offset + allocationInfo.SizeInBytes, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
	CHECK(newOffset <= Size);

	TextureResource resource = nullptr;
	static constexpr const DXGI_FORMAT* noCastableFormats = nullptr;
	CHECK_RESULT(Device->GetDevice()->CreatePlacedResource2(Heap, Offset, &textureDescription, ToD3D12(initialLayout),
															IsDepthFormat(handle.GetFormat()) ? &depthClear : nullptr, 0, noCastableFormats, IID_PPV_ARGS(&resource)));
	Offset = newOffset;

#if DEBUG
	CHECK_RESULT(resource->SetPrivateData(D3DDebugObjectName, static_cast<uint32>(name.GetLength()), name.GetData()));
#else
	(void)name;
#endif
	return resource;
}

void ViewHeap::Create(usize viewCount, ViewHeapType type, bool shaderVisible, const GpuDevice* device)
{
	static constexpr usize oneEmptyView = 1;

	Count = viewCount;
	Index = oneEmptyView;

	const D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDescriptor =
	{
		.Type = ToD3D12(type),
		.NumDescriptors = static_cast<uint32>(viewCount),
		.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
		.NodeMask = 0,
	};
	CHECK_RESULT(device->GetDevice()->CreateDescriptorHeap(&descriptorHeapDescriptor, IID_PPV_ARGS(&Heap)));
	ViewSize = device->GetDevice()->GetDescriptorHandleIncrementSize(descriptorHeapDescriptor.Type);
	CpuBase = Heap->GetCPUDescriptorHandleForHeapStart().ptr;
}

void ViewHeap::Destroy()
{
	SAFE_RELEASE(Heap);
	Count = 0;
	ViewSize = 0;
	CpuBase = 0;
	Index = 0;
}

usize ViewHeap::AllocateIndex()
{
	CHECK(Index < Count);
	return Index++;
}

void ViewHeap::Reset()
{
	Index = 0;
}

CpuView ViewHeap::GetCpu(usize index) const
{
	return Heap->GetCPUDescriptorHandleForHeapStart().ptr + index * ViewSize;
}

GpuView ViewHeap::GetGpu(usize index) const
{
	return Heap->GetGPUDescriptorHandleForHeapStart().ptr + index * ViewSize;
}

ID3D12DescriptorHeap* ViewHeap::GetHeap() const
{
	CHECK(Heap);
	return Heap;
}
