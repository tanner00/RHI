#include "ViewHeap.hpp"
#include "GpuDevice.hpp"
#include "PrivateCommon.hpp"

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

void ViewHeap::Create(ID3D12Device11* device, uint32 viewCount, ViewHeapType type, bool shaderVisible)
{
	Count = viewCount;
	Index = 0;

	const D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDescriptor =
	{
		.Type = ToD3D12(type),
		.NumDescriptors = Count,
		.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
		.NodeMask = 0,
	};
	CHECK_RESULT(device->CreateDescriptorHeap(&descriptorHeapDescriptor, IID_PPV_ARGS(&Heap)));
	ViewSize = device->GetDescriptorHandleIncrementSize(descriptorHeapDescriptor.Type);
}

void ViewHeap::Destroy()
{
	SAFE_RELEASE(Heap);
	ViewSize = 0;
	Count = 0;
	Index = 0;
}

uint32 ViewHeap::AllocateIndex()
{
	CHECK(Index < Count);
	return ++Index;
}

void ViewHeap::Reset()
{
	Index = 0;
}

CpuView ViewHeap::GetCpu(uint32 index) const
{
	return Heap->GetCPUDescriptorHandleForHeapStart().ptr + index * ViewSize;
}

GpuView ViewHeap::GetGpu(uint32 index) const
{
	return Heap->GetGPUDescriptorHandleForHeapStart().ptr + index * ViewSize;
}

ID3D12DescriptorHeap* ViewHeap::GetHeap() const
{
	CHECK(Heap);
	return Heap;
}
