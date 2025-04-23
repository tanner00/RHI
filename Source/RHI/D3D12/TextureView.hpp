#pragma once

#include "Base.hpp"

#include "RHI/TextureView.hpp"

namespace RHI::D3D12
{

class TextureView final : public TextureViewDescription, NoCopy
{
public:
	TextureView(const TextureViewDescription& description, Device* device);

	D3D12_CPU_DESCRIPTOR_HANDLE GetCpu() const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetGpu() const;

	uint32 HeapIndex;
	Device* Device;
};

}
