#pragma once

#include "Buffer.hpp"
#include "ComputePipeline.hpp"
#include "GraphicsContext.hpp"
#include "GraphicsPipeline.hpp"
#include "Sampler.hpp"
#include "Shader.hpp"
#include "ViewHeap.hpp"

class GpuDevice : public NoCopy
{
public:
	explicit GpuDevice(const Platform::Window* window);
	~GpuDevice();

	Buffer CreateBuffer(StringView name, BufferDescription&& description);
	Buffer CreateBuffer(StringView name, const void* staticData, BufferDescription&& description);
	Texture CreateTexture(StringView name, BarrierLayout initialLayout, TextureDescription&& description, TextureResource existingResource = nullptr);
	Sampler CreateSampler(SamplerDescription&& description);
	Shader CreateShader(ShaderDescription&& description);
	GraphicsPipeline CreatePipeline(StringView name, GraphicsPipelineDescription&& description);
	ComputePipeline CreatePipeline(StringView name, ComputePipelineDescription&& description);

	void DestroyBuffer(Buffer* buffer);
	void DestroyTexture(Texture* texture);
	void DestroySampler(Sampler* sampler);
	void DestroyShader(Shader* shader);
	void DestroyPipeline(Pipeline* pipeline);

	GraphicsContext CreateGraphicsContext();

	void Write(const Buffer& buffer, const void* data);
	void Write(const Texture& texture, const void* data);

	uint32 Get(const Buffer& buffer);
	uint32 Get(const Texture& texture);
	uint32 Get(const Sampler& sampler);

	void Submit(const GraphicsContext& context);
	void Present();
	void WaitForIdle();

	void ReleaseAllDeletes();

	void ResizeSwapChain(uint32 width, uint32 height);
	TextureResource GetSwapChainResource(usize backBufferIndex) const;
	usize GetFrameIndex() const;

private:
	void FlushUploads();

	void ReleaseFrameDeletes();
	void AddPendingDelete(IUnknown* pendingDelete);

	ID3D12Device11* GetDevice() const { CHECK(Device); return Device; }
	IDXGISwapChain4* GetSwapChain() const { CHECK(SwapChain); return SwapChain; }
	double GetTimestampFrequency() const { return TimestampFrequency; }

	ID3D12Device11* Device;
	IDXGISwapChain4* SwapChain;

	ID3D12CommandQueue* GraphicsQueue;

	ID3D12Fence1* FrameFence;
	uint64 FrameFenceValues[FramesInFlight];

	ID3D12CommandAllocator* UploadCommandAllocators[FramesInFlight];
	ID3D12GraphicsCommandList10* UploadCommandList;

	usize HandleIndex;
	HashTable<Buffer, D3D12Buffer> Buffers;
	HashTable<Texture, D3D12Texture> Textures;
	HashTable<Sampler, D3D12Sampler> Samplers;
	HashTable<Shader, D3D12Shader> Shaders;
	HashTable<Pipeline, D3D12Pipeline> Pipelines;

	Array<Array<IUnknown*>> PendingDeletes;
	Array<UploadPair<Buffer>> PendingBufferUploads;
	Array<UploadPair<Texture>> PendingTextureUploads;

	ViewHeap ConstantBufferShaderResourceUnorderedAccessViewHeap;
	ViewHeap RenderTargetViewHeap;
	ViewHeap DepthStencilViewHeap;
	ViewHeap SamplerViewHeap;

	double TimestampFrequency;

	friend GraphicsContext;
};
