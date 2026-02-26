#pragma once

#include "Base.hpp"
#include "ViewHeap.hpp"

#include "RHI/RHI.hpp"

namespace RHI::D3D12
{

class Device : public NoCopy
{
public:
	explicit Device(const Platform::Window* window);
	~Device();

	AccelerationStructure* Create(const AccelerationStructureDescription& description);
	BufferView* Create(const BufferViewDescription& description);
	ComputePipeline* Create(const ComputePipelineDescription& description);
	GraphicsContext* Create(const GraphicsContextDescription& description);
	GraphicsPipeline* Create(const GraphicsPipelineDescription& description);
	Heap* Create(const HeapDescription& description);
	Resource* Create(const ResourceDescription& description);
	Sampler* Create(const SamplerDescription& description);
	Shader* Create(const ShaderDescription& description);
	TextureView* Create(const TextureViewDescription& description);

	void Destroy(AccelerationStructure* accelerationStructure) const;
	void Destroy(BufferView* bufferView) const;
	void Destroy(ComputePipeline* computePipeline) const;
	void Destroy(GraphicsContext* graphicsContext) const;
	void Destroy(GraphicsPipeline* graphicsPipeline) const;
	void Destroy(Heap* heap);
	void Destroy(Resource* resource) const;
	void Destroy(Sampler* sampler) const;
	void Destroy(Shader* shader) const;
	void Destroy(TextureView* textureView) const;

	void Write(Resource* write, const ResourceDescription& format, const void* data) const;

	void Submit(const GraphicsContext* context) const;
	void Present();
	void WaitForIdle();

	void ResizeSwapChain(uint32 width, uint32 height);

	usize GetFrameIndex() const;

	usize GetResourceSize(const ResourceDescription& description) const;
	usize GetResourceAlignment(const ResourceDescription& description) const;

	AccelerationStructureSize GetAccelerationStructureSize(const AccelerationStructureGeometry& geometry) const;
	AccelerationStructureSize GetAccelerationStructureSize(const Buffer& instances) const;
	usize GetAccelerationStructureInstanceSize();

	D3D12_CPU_DESCRIPTOR_HANDLE GetCpu(usize index, ViewType type) const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetGpu(usize index, ViewType type) const;

	ID3D12Resource2* GetSwapChainResource(usize backBufferIndex) const;

	ID3D12Device11* Native;
	IDXGISwapChain4* SwapChain;

	ID3D12CommandQueue* GraphicsQueue;

	ID3D12Fence1* FrameFence;
	uint64 FrameFenceValues[FramesInFlight];

	ViewHeap ConstantBufferShaderResourceUnorderedAccessViewHeap;
	ViewHeap RenderTargetViewHeap;
	ViewHeap DepthStencilViewHeap;
	ViewHeap SamplerViewHeap;

	double TimeStampFrequency;
};

}
