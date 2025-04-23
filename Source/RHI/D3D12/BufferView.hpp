#pragma once

#include "Base.hpp"

#include "RHI/BufferView.hpp"

namespace RHI::D3D12
{

class BufferView final : public BufferViewDescription, NoCopy
{
public:
	BufferView(const BufferViewDescription& description, Device* device);

	D3D12_CPU_DESCRIPTOR_HANDLE GetCpu() const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetGpu() const;

	uint32 HeapIndex;
	Device* Device;
};

}
