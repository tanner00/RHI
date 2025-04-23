#include "ViewHeap.hpp"
#include "Device.hpp"

namespace RHI::D3D12
{

static D3D12_DESCRIPTOR_HEAP_TYPE To(ViewHeapType type)
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

void ViewHeap::Create(uint32 viewCount, ViewHeapType type, bool shaderVisible, const Device* device)
{
	Count = viewCount;
	Index = 0;

	const D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDescriptor =
	{
		.Type = To(type),
		.NumDescriptors = Count,
		.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
		.NodeMask = 0,
	};
	CHECK_RESULT(device->Native->CreateDescriptorHeap(&descriptorHeapDescriptor, IID_PPV_ARGS(&Native)));
	ViewSize = device->Native->GetDescriptorHandleIncrementSize(descriptorHeapDescriptor.Type);
}

void ViewHeap::Destroy()
{
	SAFE_RELEASE(Native);
	ViewSize = 0;
	Count = 0;
	Index = 0;
}

uint32 ViewHeap::AllocateIndex()
{
	++Index;
	CHECK(Index < Count);
	return Index;
}

void ViewHeap::Reset()
{
	Index = 0;
}

D3D12_CPU_DESCRIPTOR_HANDLE ViewHeap::GetCpu(usize index) const
{
	return D3D12_CPU_DESCRIPTOR_HANDLE { Native->GetCPUDescriptorHandleForHeapStart().ptr + index * ViewSize };
}

D3D12_GPU_DESCRIPTOR_HANDLE ViewHeap::GetGpu(usize index) const
{
	return D3D12_GPU_DESCRIPTOR_HANDLE { Native->GetGPUDescriptorHandleForHeapStart().ptr + index * ViewSize };
}

}
