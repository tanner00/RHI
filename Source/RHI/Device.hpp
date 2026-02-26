#pragma once

#include "AccelerationStructure.hpp"
#include "BufferView.hpp"
#include "ComputePipeline.hpp"
#include "Forward.hpp"
#include "GraphicsContext.hpp"
#include "GraphicsPipeline.hpp"
#include "Heap.hpp"
#include "Resource.hpp"
#include "Sampler.hpp"
#include "Shader.hpp"
#include "TextureView.hpp"

namespace RHI
{

inline constexpr usize FramesInFlight = 2;

class Device : public NoCopy
{
public:
	explicit Device(const Platform::Window* window);
	~Device();

	AccelerationStructure Create(const AccelerationStructureDescription& description) const;
	BufferView Create(const BufferViewDescription& description) const;
	ComputePipeline Create(const ComputePipelineDescription& description) const;
	GraphicsContext Create(const GraphicsContextDescription& description) const;
	GraphicsPipeline Create(const GraphicsPipelineDescription& description) const;
	Heap Create(const HeapDescription& description) const;
	Resource Create(const ResourceDescription& description) const;
	Sampler Create(const SamplerDescription& description) const;
	Shader Create(const ShaderDescription& description) const;
	TextureView Create(const TextureViewDescription& description) const;

	void Destroy(AccelerationStructure* accelerationStructure) const;
	void Destroy(BufferView* bufferView) const;
	void Destroy(ComputePipeline* computePipeline) const;
	void Destroy(GraphicsContext* graphicsContext) const;
	void Destroy(GraphicsPipeline* graphicsPipeline) const;
	void Destroy(Heap* heap) const;
	void Destroy(Resource* resource) const;
	void Destroy(Sampler* sampler) const;
	void Destroy(Shader* shader) const;
	void Destroy(TextureView* textureView) const;

	uint32 Get(const AccelerationStructure& accelerationStructure);
	uint32 Get(const BufferView& buffer) const;
	uint32 Get(const Sampler& sampler) const;
	uint32 Get(const TextureView& texture) const;

	void Write(const Resource* write, const ResourceDescription& format, const void* data) const;
	void Write(const Resource* write, const void* data) const { Write(write, *write, data); }

	void Submit(const GraphicsContext& context) const;
	void Present();
	void WaitForIdle();

	void ResizeSwapChain(uint32 width, uint32 height);

	usize GetFrameIndex() const;

	usize GetResourceSize(const ResourceDescription& description) const;
	usize GetResourceAlignment(const ResourceDescription& description) const;

	AccelerationStructureSize GetAccelerationStructureSize(const AccelerationStructureGeometry& geometry) const;
	AccelerationStructureSize GetAccelerationStructureSize(const Buffer& instances) const;
	usize GetAccelerationStructureInstanceSize();

private:
	RHI_BACKEND(Device)* Backend;
};

}
