#pragma once

#include "Common.hpp"

#include "Luft/Base.hpp"
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

	void Create(ID3D12Device11* device, usize viewCount, ViewHeapType type, bool shaderVisible);
	void Destroy();

	usize AllocateIndex();
	void Reset();

	CpuView GetCpu(usize index) const;
	GpuView GetGpu(usize index) const;

	ID3D12DescriptorHeap* GetHeap() const;

private:
	ID3D12DescriptorHeap* Heap;
	usize Count;
	usize ViewSize;
	usize CpuBase;
	usize Index;
};
