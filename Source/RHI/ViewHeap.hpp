#pragma once

#include "Common.hpp"

#include "Luft/NoCopy.hpp"

enum class ViewHeapType
{
	ConstantBufferShaderResourceUnorderedAccess,
	Sampler,
	RenderTarget,
	DepthStencil,
};

class ViewHeap : public NoCopy
{
public:
	ViewHeap() = default;

	void Create(ID3D12Device11* device, uint32 viewCount, ViewHeapType type, bool shaderVisible);
	void Destroy();

	uint32 AllocateIndex();
	void Reset();

	CpuView GetCpu(uint32 index) const;
	GpuView GetGpu(uint32 index) const;

	ID3D12DescriptorHeap* GetHeap() const;

private:
	ID3D12DescriptorHeap* Heap;
	usize ViewSize;
	uint32 Count;
	uint32 Index;
};
