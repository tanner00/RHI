#include "Device.hpp"
#include "Allocator.hpp"

#if RHI_D3D12
#include "D3D12/AccelerationStructure.hpp"
#include "D3D12/BufferView.hpp"
#include "D3D12/ComputePipeline.hpp"
#include "D3D12/Device.hpp"
#include "D3D12/GraphicsContext.hpp"
#include "D3D12/GraphicsPipeline.hpp"
#include "D3D12/Heap.hpp"
#include "D3D12/Resource.hpp"
#include "D3D12/Sampler.hpp"
#include "D3D12/TextureView.hpp"
#endif

namespace RHI
{

Device::Device(const Platform::Window* window)
	: Backend(Allocator->Create<RHI_BACKEND(Device)>(window))
{
}

Device::~Device()
{
	Allocator->Destroy(Backend);
	Backend = nullptr;
}

AccelerationStructure Device::Create(const AccelerationStructureDescription& description) const
{
	return AccelerationStructure(description, Backend->Create(description));
}

BufferView Device::Create(const BufferViewDescription& description) const
{
	return BufferView(description, Backend->Create(description));
}

ComputePipeline Device::Create(const ComputePipelineDescription& description) const
{
	return ComputePipeline(description, Backend->Create(description));
}

GraphicsContext Device::Create(const GraphicsContextDescription& description) const
{
	return GraphicsContext(description, Backend->Create(description));
}

GraphicsPipeline Device::Create(const GraphicsPipelineDescription& description) const
{
	return GraphicsPipeline(description, Backend->Create(description));
}

Heap Device::Create(const HeapDescription& description) const
{
	return Heap(description, Backend->Create(description));
}

Resource Device::Create(const ResourceDescription& description) const
{
	return Resource(description, Backend->Create(description));
}

Sampler Device::Create(const SamplerDescription& description) const
{
	return Sampler(description, Backend->Create(description));
}

Shader Device::Create(const ShaderDescription& description) const
{
	return Shader(description, Backend->Create(description));
}

TextureView Device::Create(const TextureViewDescription& description) const
{
	return TextureView(description, Backend->Create(description));
}

void Device::Destroy(AccelerationStructure* accelerationStructure) const
{
	Backend->Destroy(accelerationStructure->Backend);
	accelerationStructure->Backend = nullptr;
}

void Device::Destroy(BufferView* bufferView) const
{
	Backend->Destroy(bufferView->Backend);
	bufferView->Backend = nullptr;
}

void Device::Destroy(ComputePipeline* computePipeline) const
{
	Backend->Destroy(computePipeline->Backend);
	computePipeline->Backend = nullptr;
}

void Device::Destroy(GraphicsContext* graphicsContext) const
{
	Backend->Destroy(graphicsContext->Backend);
	graphicsContext->Backend = nullptr;
}

void Device::Destroy(GraphicsPipeline* graphicsPipeline) const
{
	Backend->Destroy(graphicsPipeline->Backend);
	graphicsPipeline->Backend = nullptr;
}

void Device::Destroy(Heap* heap) const
{
	Backend->Destroy(heap->Backend);
	heap->Backend = nullptr;
}

void Device::Destroy(Resource* resource) const
{
	Backend->Destroy(resource->Backend);
	resource->Backend = nullptr;
}

void Device::Destroy(Sampler* sampler) const
{
	Backend->Destroy(sampler->Backend);
	sampler->Backend = nullptr;
}

void Device::Destroy(Shader* shader) const
{
	Backend->Destroy(shader->Backend);
	shader->Backend = nullptr;
}

void Device::Destroy(TextureView* textureView) const
{
	Backend->Destroy(textureView->Backend);
	textureView->Backend = nullptr;
}

uint32 Device::Get(const AccelerationStructure& accelerationStructure)
{
	return accelerationStructure.Backend->HeapIndex;
}

uint32 Device::Get(const BufferView& buffer) const
{
	return buffer.Backend->HeapIndex;
}

uint32 Device::Get(const Sampler& sampler) const
{
	return sampler.Backend->HeapIndex;
}

uint32 Device::Get(const TextureView& texture) const
{
	return texture.Backend->HeapIndex;
}

void Device::Write(const Resource* write, const ResourceDescription& format, const void* data) const
{
	Backend->Write(write->Backend, format, data);
}

void Device::Submit(const GraphicsContext& context) const
{
	Backend->Submit(context.Backend);
}

void Device::Present()
{
	Backend->Present();
}

void Device::WaitForIdle()
{
	Backend->WaitForIdle();
}

void Device::ResizeSwapChain(uint32 width, uint32 height)
{
	Backend->ResizeSwapChain(width, height);
}

usize Device::GetFrameIndex() const
{
	return Backend->GetFrameIndex();
}

usize Device::GetResourceSize(const ResourceDescription& description) const
{
	return Backend->GetResourceSize(description);
}

usize Device::GetResourceAlignment(const ResourceDescription& description) const
{
	return Backend->GetResourceAlignment(description);
}

AccelerationStructureSize Device::GetAccelerationStructureSize(const AccelerationStructureGeometry& geometry) const
{
	return Backend->GetAccelerationStructureSize(geometry);
}

AccelerationStructureSize Device::GetAccelerationStructureSize(const Buffer& instances) const
{
	return Backend->GetAccelerationStructureSize(instances);
}

usize Device::GetAccelerationStructureInstanceSize()
{
	return Backend->GetAccelerationStructureInstanceSize();
}

}
