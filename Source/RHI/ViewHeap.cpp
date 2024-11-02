#include "ViewHeap.hpp"
#include "GpuDevice.hpp"

#include "Luft/Math.hpp"

#include "D3D12/d3d12.h"

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
