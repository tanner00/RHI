#include "GraphicsContext.hpp"
#include "ComputePipeline.hpp"
#include "GraphicsPipeline.hpp"
#include "TextureView.hpp"

#include "D3D12/Base.hpp"
#include "D3D12/GraphicsContext.hpp"

namespace RHI
{

void GraphicsContext::Begin() const
{
	Backend->Begin();
}

void GraphicsContext::End()
{
	Backend->End();
}

void GraphicsContext::SetViewport(uint32 width, uint32 height) const
{
	Backend->SetViewport(width, height);
}

void GraphicsContext::SetRenderTarget(const TextureView& renderTarget) const
{
	Backend->SetRenderTarget(renderTarget.Backend);
}

void GraphicsContext::SetRenderTarget(const TextureView& renderTarget, const TextureView& depthStencil) const
{
	Backend->SetRenderTarget(renderTarget.Backend, depthStencil.Backend);
}

void GraphicsContext::SetRenderTargets(const ArrayView<const TextureView>& renderTargets, const TextureView& depthStencil) const
{
	CHECK(renderTargets.GetLength() < RHI_BACKEND(MaxRenderTargetCount));

	const RHI_BACKEND(TextureView)* renderTargetBackends[RHI_BACKEND(MaxRenderTargetCount)] = {};
	for (usize renderTargetIndex = 0; renderTargetIndex < renderTargets.GetLength(); ++renderTargetIndex)
	{
		renderTargetBackends[renderTargetIndex] = renderTargets[renderTargetIndex].Backend;
	}
	Backend->SetRenderTargets(ArrayView(renderTargetBackends, renderTargets.GetLength()), depthStencil.Backend);
}

void GraphicsContext::SetDepthRenderTarget(const TextureView& depthStencil) const
{
	Backend->SetDepthRenderTarget(depthStencil.Backend);
}

void GraphicsContext::ClearRenderTarget(const TextureView& renderTarget) const
{
	Backend->ClearRenderTarget(renderTarget.Backend);
}

void GraphicsContext::ClearDepthStencil(const TextureView& depthStencil) const
{
	Backend->ClearDepthStencil(depthStencil.Backend);
}

void GraphicsContext::SetPipeline(const GraphicsPipeline& pipeline)
{
	Backend->SetPipeline(pipeline.Backend);
}

void GraphicsContext::SetPipeline(const ComputePipeline& pipeline)
{
	Backend->SetPipeline(pipeline.Backend);
}

void GraphicsContext::SetVertexBuffer(usize slot, const SubBuffer& vertexBuffer) const
{
	Backend->SetVertexBuffer(slot, vertexBuffer);
}

void GraphicsContext::SetIndexBuffer(const SubBuffer& indexBuffer) const
{
	Backend->SetIndexBuffer(indexBuffer);
}

void GraphicsContext::SetConstantBuffer(StringView name, const Resource& buffer, usize offset) const
{
	Backend->SetConstantBuffer(name, buffer.Backend, offset);
}

void GraphicsContext::SetRootConstants(const void* data) const
{
	Backend->SetRootConstants(data);
}

void GraphicsContext::Draw(usize vertexCount) const
{
	Backend->Draw(vertexCount);
}

void GraphicsContext::DrawIndexed(usize indexCount) const
{
	Backend->DrawIndexed(indexCount);
}

void GraphicsContext::Dispatch(uint32 threadGroupCountX, uint32 threadGroupCountY, uint32 threadGroupCountZ) const
{
	Backend->Dispatch(threadGroupCountX, threadGroupCountY, threadGroupCountZ);
}

void GraphicsContext::Copy(const Resource& destination, const Resource& source) const
{
	Backend->Copy(destination.Backend, source.Backend);
}

void GraphicsContext::GlobalBarrier(BarrierPair<BarrierStage> stage, BarrierPair<BarrierAccess> access) const
{
	Backend->GlobalBarrier(stage, access);
}

void GraphicsContext::BufferBarrier(BarrierPair<BarrierStage> stage, BarrierPair<BarrierAccess> access, const Resource& buffer) const
{
	Backend->BufferBarrier(stage, access, buffer.Backend);
}

void GraphicsContext::TextureBarrier(BarrierPair<BarrierStage> stage,
									 BarrierPair<BarrierAccess> access,
									 BarrierPair<BarrierLayout> layout,
									 const Resource& texture) const
{
	Backend->TextureBarrier(stage, access, layout, texture.Backend);
}

void GraphicsContext::BuildAccelerationStructure(const AccelerationStructureGeometry& geometry,
												 const Resource& scratchResource,
												 const Resource& resultResource) const
{
	return Backend->BuildAccelerationStructure(geometry, scratchResource.Backend, resultResource.Backend);
}

void GraphicsContext::BuildAccelerationStructure(const Buffer& instances,
												 const Resource& scratchResource,
												 const Resource& resultResource) const
{
	return Backend->BuildAccelerationStructure(instances, scratchResource.Backend, resultResource.Backend);
}

double GraphicsContext::GetMostRecentGpuTime() const
{
	return Backend->MostRecentGpuTime;
}

}
