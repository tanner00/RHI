#pragma once

#include "Common.hpp"
#include "GraphicsContext.hpp"
#include "Heap.hpp"

#include "Luft/Array.hpp"
#include "Luft/Base.hpp"
#include "Luft/NoCopy.hpp"

namespace Platform
{
struct Window;
}

class Buffer
{
public:
	BufferResource Resources[FramesInFlight];
	usize HeapIndices[FramesInFlight][static_cast<usize>(DescriptorType::Count)];

	BufferResource GetOnlyBufferResource() const
	{
#if DEBUG
		usize validResourceCount = 0;
		for (BufferResource resource : Resources)
		{
			if (resource) ++validResourceCount;
		}
		CHECK(validResourceCount == 1);
#endif
		CHECK(Resources[0]);
		return Resources[0];
	}

	BufferResource GetBufferResource(usize index, bool stream) const
	{
		CHECK(index < FramesInFlight);
		const BufferResource current = stream ? Resources[index] : Resources[0];
		CHECK(current);
		return current;
	}
};

class Texture
{
public:
	TextureResource Resource;
	usize HeapIndices[static_cast<usize>(DescriptorType::Count)];

	TextureResource GetTextureResource() const { CHECK(Resource); return Resource; }
};

class Sampler
{
public:
	usize HeapIndex;
};

struct Shader
{
	IDxcBlob* Blob;
	ID3D12ShaderReflection* Reflection;
};

struct GraphicsPipeline
{
	HashTable<String, usize> Parameters;
	ID3D12RootSignature* RootSignature;
	ID3D12PipelineState* PipelineState;
};

template<typename T> requires IsSame<RemoveCvType<T>, BufferHandle>::Value || IsSame<RemoveCvType<T>, TextureHandle>::Value
struct UploadPair
{
	ID3D12Resource2* Source;
	T Destination;
};

class GpuDevice : public NoCopy
{
public:
	explicit GpuDevice(const Platform::Window* window);
	~GpuDevice();

	BufferHandle CreateBuffer(StringView name, const BufferDescription& description);
	BufferHandle CreateBuffer(StringView name, const void* staticData, const BufferDescription& description);
	TextureHandle CreateTexture(StringView name, BarrierLayout initialLayout, const TextureDescription& description, TextureResource existingResource = nullptr);
	SamplerHandle CreateSampler(const SamplerDescription& description);
	ShaderHandle CreateShader(const ShaderDescription& description);
	GraphicsPipelineHandle CreateGraphicsPipeline(StringView name, const GraphicsPipelineDescription& description);

	void DestroyBuffer(BufferHandle& handle);
	void DestroyTexture(TextureHandle& handle);
	void DestroySampler(SamplerHandle& handle);
	void DestroyShader(ShaderHandle& handle);
	void DestroyGraphicsPipeline(GraphicsPipelineHandle& handle);

	GraphicsContext CreateGraphicsContext();

	void Write(const BufferHandle& handle, const void* data);
	void Write(const TextureHandle& handle, const void* data);
	void WriteCubemap(const TextureHandle& handle, const Array<uint8*>& faces);

	void Submit(const GraphicsContext& context);
	void Present();
	void WaitForIdle();

	void ReleaseAllDeletes();

	void ResizeSwapChain(uint32 width, uint32 height);
	TextureResource GetSwapChainResource(usize backBufferIndex) const;
	usize GetFrameIndex() const;

private:
	void ReleaseFrameDeletes();
	void AddPendingDelete(IUnknown* pendingDelete);

	void EnsureConstantBufferDescriptor(const BufferHandle& handle);
	void EnsureShaderResourceDescriptor(const BufferHandle& handle);

	void EnsureShaderResourceDescriptor(const TextureHandle& handle);
	void EnsureRenderTargetDescriptor(const TextureHandle& handle);
	void EnsureDepthStencilDescriptor(const TextureHandle& handle);

	ID3D12Device11* GetDevice() const;
	IDXGISwapChain4* GetSwapChain() const;

	ID3D12Device11* Device;
	IDXGISwapChain4* SwapChain;

	ID3D12CommandQueue* GraphicsQueue;

	ID3D12Fence1* FrameFence;
	uint64 FrameFenceValues[FramesInFlight];

	usize FrameIndex;

	ID3D12CommandAllocator* UploadCommandAllocators[FramesInFlight];
	ID3D12GraphicsCommandList10* UploadCommandList;

	usize HandleIndex;
	HashTable<usize, Buffer> Buffers;
	HashTable<usize, Texture> Textures;
	HashTable<usize, Sampler> Samplers;
	HashTable<usize, Shader> Shaders;
	HashTable<usize, GraphicsPipeline> GraphicsPipelines;

	Array<Array<IUnknown*>> PendingDeletes;
	Array<UploadPair<BufferHandle>> PendingBufferUploads;
	Array<UploadPair<TextureHandle>> PendingTextureUploads;

	DescriptorHeap ConstantBufferShaderResourceUnorderedAccessDescriptorHeap;
	DescriptorHeap RenderTargetDescriptorHeap;
	DescriptorHeap DepthStencilDescriptorHeap;
	DescriptorHeap SamplerDescriptorHeap;

	GpuHeap DefaultHeap;
	GpuHeap UploadHeap;

	friend DescriptorHeap;
	friend GraphicsContext;
	friend GpuHeap;
};
