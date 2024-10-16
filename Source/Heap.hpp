#pragma once

#include "GraphicsContext.hpp"
#include "Common.hpp"

#include "Base.hpp"
#include "NoCopy.hpp"

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
	TextureResource AllocateTexture(usize width, usize height, const TextureHandle& handle, BarrierLayout initialLayout, StringView name);

private:
	ID3D12Heap1* Heap;
	usize Size;
	usize Offset;

	GpuDevice* Device;
};

enum class DescriptorHeapType
{
	ConstantBufferShaderResourceUnorderedAccess,
	Sampler,
	RenderTarget,
	DepthStencil,
};

class DescriptorHeap : public NoCopy
{
public:
	DescriptorHeap() = default;

	void Create(usize descriptorCount, DescriptorHeapType type, bool shaderVisible, const GpuDevice* device);
	void Destroy();

	usize AllocateIndex();
	void Reset();

	CpuDescriptor GetCpuDescriptor(usize index) const;
	GpuDescriptor GetGpuDescriptor(usize index) const;

	ID3D12DescriptorHeap* GetHeap();

private:
	ID3D12DescriptorHeap* Heap;
	usize Count;
	usize DescriptorSize;
	usize CpuBase;
	usize Index;
};
