#include "Sampler.hpp"
#include "Convert.hpp"

namespace RHI::D3D12
{

Sampler::Sampler(const SamplerDescription& description, Device* device)
	: SamplerDescription(description)
	, HeapIndex(device->SamplerViewHeap.AllocateIndex())
{
	const D3D12_SAMPLER_DESC2 nativeDescription = To(description);
	device->Native->CreateSampler2(&nativeDescription, D3D12_CPU_DESCRIPTOR_HANDLE { device->SamplerViewHeap.GetCpu(HeapIndex) });
}

}
