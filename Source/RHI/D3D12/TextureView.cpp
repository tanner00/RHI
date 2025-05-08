#include "TextureView.hpp"
#include "Convert.hpp"
#include "Device.hpp"
#include "Resource.hpp"

namespace RHI::D3D12
{

TextureView::TextureView(const TextureViewDescription& description, D3D12::Device* device)
	: TextureViewDescription(description)
	, Device(device)
{
	CHECK(Resource.IsValid());
	const D3D12::Resource* backendResource = Resource.Backend;

	switch (Type)
	{
	case ViewType::ShaderResource:
	{
		HeapIndex = device->ConstantBufferShaderResourceUnorderedAccessViewHeap.AllocateIndex();

		const D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceDescription = To<ViewType::ShaderResource>(description);
		device->Native->CreateShaderResourceView(backendResource->Native, &shaderResourceDescription, GetCpu());
		break;
	}
	case ViewType::RenderTarget:
	{
		HeapIndex = device->RenderTargetViewHeap.AllocateIndex();

		const D3D12_RENDER_TARGET_VIEW_DESC renderTargetDescription = To<ViewType::RenderTarget>(description);
		device->Native->CreateRenderTargetView(backendResource->Native, &renderTargetDescription, GetCpu());
		break;
	}
	case ViewType::DepthStencil:
	{
		HeapIndex = device->DepthStencilViewHeap.AllocateIndex();

		const D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDescription = To<ViewType::DepthStencil>(description);
		device->Native->CreateDepthStencilView(backendResource->Native, &depthStencilDescription, GetCpu());
		break;
	}
	default:
		CHECK(false);
	}
}

D3D12_CPU_DESCRIPTOR_HANDLE TextureView::GetCpu() const
{
	return Device->GetCpu(HeapIndex, Type);
}

D3D12_GPU_DESCRIPTOR_HANDLE TextureView::GetGpu() const
{
	return Device->GetGpu(HeapIndex, Type);
}

}
