#pragma once

#include "GraphicsContext.hpp"
#include "Common.hpp"

#include "Luft/Base.hpp"
#include "Luft/NoCopy.hpp"

enum class GpuHeapType
{
	Default,
	Upload,
};

class GpuHeap : public NoCopy
{
public:
	GpuHeap() = default;

	void Create(usize size, GpuHeapType type, GpuDevice* device);
	void Destroy();

	BufferResource AllocateBuffer(usize size, StringView name);
	TextureResource AllocateTexture(const TextureHandle& handle, BarrierLayout initialLayout, StringView name);

private:
	ID3D12Heap1* Heap;
	usize Size;
	usize Offset;

	GpuDevice* Device;
};

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

	void Create(usize viewCount, ViewHeapType type, bool shaderVisible, const GpuDevice* device);
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
