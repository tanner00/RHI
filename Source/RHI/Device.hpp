#pragma once

#include "AccelerationStructure.hpp"
#include "BufferView.hpp"
#include "ComputePipeline.hpp"
#include "Forward.hpp"
#include "GraphicsContext.hpp"
#include "GraphicsPipeline.hpp"
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

	AccelerationStructure Create(const AccelerationStructureDescription& description);
	BufferView Create(const BufferViewDescription& description);
	ComputePipeline Create(const ComputePipelineDescription& description);
	GraphicsContext Create(const GraphicsContextDescription& description);
	GraphicsPipeline Create(const GraphicsPipelineDescription& description);
	Resource Create(const ResourceDescription& description);
	Sampler Create(const SamplerDescription& description);
	Shader Create(const ShaderDescription& description);
	TextureView Create(const TextureViewDescription& description);

	void Destroy(AccelerationStructure* accelerationStructure) const;
	void Destroy(BufferView* bufferView) const;
	void Destroy(ComputePipeline* computePipeline) const;
	void Destroy(GraphicsContext* graphicsContext) const;
	void Destroy(GraphicsPipeline* graphicsPipeline) const;
	void Destroy(Resource* resource) const;
	void Destroy(Sampler* sampler) const;
	void Destroy(Shader* shader) const;
	void Destroy(TextureView* textureView) const;

	uint32 Get(const AccelerationStructure& accelerationStructure);
	uint32 Get(const BufferView& buffer) const;
	uint32 Get(const Sampler& sampler) const;
	uint32 Get(const TextureView& texture) const;

	void Write(const Resource* resource, const void* data);

	void Submit(const GraphicsContext& context);
	void Present();
	void WaitForIdle();

	void ReleaseAllDestroys();

	void ResizeSwapChain(uint32 width, uint32 height);

	usize GetFrameIndex() const;

	AccelerationStructureSize GetAccelerationStructureSize(const AccelerationStructureGeometry& geometry) const;
	AccelerationStructureSize GetAccelerationStructureSize(const Buffer& instances) const;
	usize GetAccelerationStructureInstanceSize();

private:
	RHI_BACKEND(Device)* Backend;
};

}
