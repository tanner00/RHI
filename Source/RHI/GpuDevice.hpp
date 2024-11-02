#pragma once

#include "Common.hpp"
#include "GraphicsContext.hpp"
#include "ViewHeap.hpp"

#include "Luft/Array.hpp"
#include "Luft/Base.hpp"
#include "Luft/NoCopy.hpp"

class D3D12Buffer
{
public:
	BufferResource GetOnlyBufferResource() const
	{
		CHECK(Resources[0] && !Resources[1]);
		return Resources[0];
	}

	BufferResource GetBufferResource(usize index, bool stream) const
	{
		CHECK(index < FramesInFlight);
		const BufferResource current = stream ? Resources[index] : Resources[0];
		CHECK(current);
		return current;
	}

	BufferResource Resources[FramesInFlight];
};

class D3D12Texture
{
public:
	TextureResource GetTextureResource() const { CHECK(Resource); return Resource; }

	TextureResource Resource;
	usize HeapIndices[static_cast<usize>(ViewType::Count)];
};

struct D3D12Sampler
{
	usize HeapIndex;
};

struct D3D12Shader
{
	IDxcBlob* Blob;
	ID3D12ShaderReflection* Reflection;
};

struct D3D12GraphicsPipeline
{
	HashTable<String, usize> Parameters;
	ID3D12RootSignature* RootSignature;
	ID3D12PipelineState* PipelineState;
};

template<typename T> requires IsSame<RemoveCvType<T>, Buffer>::Value || IsSame<RemoveCvType<T>, Texture>::Value
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

	Buffer CreateBuffer(StringView name, const BufferDescription& description);
	Buffer CreateBuffer(StringView name, const void* staticData, const BufferDescription& description);
	Texture CreateTexture(StringView name, BarrierLayout initialLayout, const TextureDescription& description, TextureResource existingResource = nullptr);
	Sampler CreateSampler(const SamplerDescription& description);
	Shader CreateShader(const ShaderDescription& description);
	GraphicsPipeline CreateGraphicsPipeline(StringView name, const GraphicsPipelineDescription& description);

	void DestroyBuffer(Buffer* buffer);
	void DestroyTexture(Texture* texture);
	void DestroySampler(Sampler* sampler);
	void DestroyShader(Shader* shader);
	void DestroyGraphicsPipeline(GraphicsPipeline* graphicsPipeline);

	GraphicsContext CreateGraphicsContext();

	void Write(const Buffer& buffer, const void* data);
	void Write(const Texture& texture, const void* data);
	void WriteCubemap(const Texture& texture, const Array<uint8*>& faces);

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

	void EnsureShaderResourceView(const Texture& texture);
	void EnsureRenderTargetView(const Texture& texture);
	void EnsureDepthStencilView(const Texture& texture);

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
	HashTable<usize, D3D12Buffer> Buffers;
	HashTable<usize, D3D12Texture> Textures;
	HashTable<usize, D3D12Sampler> Samplers;
	HashTable<usize, D3D12Shader> Shaders;
	HashTable<usize, D3D12GraphicsPipeline> GraphicsPipelines;

	Array<Array<IUnknown*>> PendingDeletes;
	Array<UploadPair<Buffer>> PendingBufferUploads;
	Array<UploadPair<Texture>> PendingTextureUploads;

	ViewHeap ConstantBufferShaderResourceUnorderedAccessViewHeap;
	ViewHeap RenderTargetViewHeap;
	ViewHeap DepthStencilViewHeap;
	ViewHeap SamplerViewHeap;

	friend ViewHeap;
	friend GraphicsContext;
};
