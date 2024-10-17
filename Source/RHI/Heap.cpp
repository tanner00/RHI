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

static D3D12_DESCRIPTOR_HEAP_TYPE ToD3D12(DescriptorHeapType type)
{
	switch (type)
	{
	case DescriptorHeapType::ConstantBufferShaderResourceUnorderedAccess:
		return D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	case DescriptorHeapType::Sampler:
		return D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
	case DescriptorHeapType::RenderTarget:
		return D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	case DescriptorHeapType::DepthStencil:
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

	const D3D12_HEAP_DESC heapDescriptor =
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
	CHECK_RESULT(device->GetDevice()->CreateHeap(&heapDescriptor, IID_PPV_ARGS(&Heap)));
}

void GpuHeap::Destroy()
{
	SAFE_RELEASE(Heap);
	Size = 0;
	Device = nullptr;
}

BufferResource GpuHeap::AllocateBuffer(usize size, StringView name)
{
	const D3D12_RESOURCE_DESC1 bufferDescriptor =
	{
		.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
		.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
		.Width = size,
		.Height = 1,
		.DepthOrArraySize = 1,
		.MipLevels = 1,
		.Format = DXGI_FORMAT_UNKNOWN,
		.SampleDesc = DefaultSampleDescriptor,
		.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
		.Flags = D3D12_RESOURCE_FLAG_NONE,
		.SamplerFeedbackMipRegion = {},
	};

	const usize newOffset = NextMultipleOf(Offset + size, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
	CHECK(newOffset <= Size);

	constexpr const DXGI_FORMAT* noCastableFormats = nullptr;
	BufferResource resource = nullptr;
	CHECK_RESULT(Device->GetDevice()->CreatePlacedResource2(Heap, Offset, &bufferDescriptor, ToD3D12(BarrierLayout::Undefined),
															nullptr, 0, noCastableFormats, IID_PPV_ARGS(&resource)));
	Offset = newOffset;

#if DEBUG
	CHECK_RESULT(resource->SetPrivateData(D3DDebugObjectName, static_cast<uint32>(name.Length), name.Buffer));
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

	const D3D12_RESOURCE_ALLOCATION_INFO allocationInfo = Device->GetDevice()->GetResourceAllocationInfo2(0, 1, &textureDescriptor, nullptr);
	const usize newOffset = NextMultipleOf(Offset + allocationInfo.SizeInBytes, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
	CHECK(newOffset <= Size);

	TextureResource resource = nullptr;
	constexpr const DXGI_FORMAT* noCastableFormats = nullptr;
	CHECK_RESULT(Device->GetDevice()->CreatePlacedResource2(Heap, Offset, &textureDescriptor, ToD3D12(initialLayout),
															IsDepthFormat(handle.GetFormat()) ? &depthClear : nullptr, 0, noCastableFormats, IID_PPV_ARGS(&resource)));
	Offset = newOffset;

#if DEBUG
	CHECK_RESULT(resource->SetPrivateData(D3DDebugObjectName, static_cast<uint32>(name.Length), name.Buffer));
#else
	(void)name;
#endif
	return resource;
}

void DescriptorHeap::Create(usize descriptorCount, DescriptorHeapType type, bool shaderVisible, const GpuDevice* device)
{
	static constexpr usize oneEmptyDescriptor = 1;

	Count = descriptorCount;
	Index = oneEmptyDescriptor;

	const D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDescriptor =
	{
		.Type = ToD3D12(type),
		.NumDescriptors = static_cast<uint32>(descriptorCount),
		.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
		.NodeMask = 0,
	};
	CHECK_RESULT(device->GetDevice()->CreateDescriptorHeap(&descriptorHeapDescriptor, IID_PPV_ARGS(&Heap)));
	DescriptorSize = device->GetDevice()->GetDescriptorHandleIncrementSize(descriptorHeapDescriptor.Type);
	CpuBase = Heap->GetCPUDescriptorHandleForHeapStart().ptr;
}

void DescriptorHeap::Destroy()
{
	SAFE_RELEASE(Heap);
	Count = 0;
	DescriptorSize = 0;
	CpuBase = 0;
	Index = 0;
}

usize DescriptorHeap::AllocateIndex()
{
	CHECK(Index < Count);
	return Index++;
}

void DescriptorHeap::Reset()
{
	Index = 0;
}

CpuDescriptor DescriptorHeap::GetCpuDescriptor(usize index) const
{
	return Heap->GetCPUDescriptorHandleForHeapStart().ptr + index * DescriptorSize;
}

GpuDescriptor DescriptorHeap::GetGpuDescriptor(usize index) const
{
	return Heap->GetGPUDescriptorHandleForHeapStart().ptr + index * DescriptorSize;
}

ID3D12DescriptorHeap* DescriptorHeap::GetHeap()
{
	CHECK(Heap);
	return Heap;
}
