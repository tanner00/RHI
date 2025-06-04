#pragma once

#include "Base.hpp"

#include "RHI/Forward.hpp"

#include "Luft/Base.hpp"
#include "Luft/NoCopy.hpp"

namespace RHI::D3D12
{

class ViewHeap : public NoCopy
{
public:
	void Create(uint32 viewCount, D3D12_DESCRIPTOR_HEAP_TYPE type, bool shaderVisible, const Device* device);
	void Destroy();

	uint32 AllocateIndex();
	void Reset();

	D3D12_CPU_DESCRIPTOR_HANDLE GetCpu(usize index) const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetGpu(usize index) const;

	ID3D12DescriptorHeap* Native;
	uint32 ViewSize;
	uint32 Count;
	uint32 Index;
};

}
