#include "GraphicsContext.hpp"
#include "ComputePipeline.hpp"
#include "GraphicsPipeline.hpp"
#include "TextureView.hpp"

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

void GraphicsContext::SetDepthRenderTarget(const TextureView& depthStencil) const
{
	Backend->SetDepthRenderTarget(depthStencil.Backend);
}

void GraphicsContext::ClearRenderTarget(const TextureView& renderTarget, Float4 color) const
{
	Backend->ClearRenderTarget(renderTarget.Backend, color);
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

void GraphicsContext::SetVertexBuffer(const Resource& vertexBuffer, usize slot, usize offset, usize size, usize stride) const
{
	Backend->SetVertexBuffer(vertexBuffer.Backend, slot, offset, size, stride);
}

void GraphicsContext::SetIndexBuffer(const Resource& indexBuffer, usize offset, usize size, usize stride) const
{
	Backend->SetIndexBuffer(indexBuffer.Backend, offset, size, stride);
}

void GraphicsContext::SetConstantBuffer(StringView name, const Resource& buffer, usize offsetIndex) const
{
	Backend->SetConstantBuffer(name, buffer.Backend, offsetIndex);
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

void GraphicsContext::Dispatch(usize threadGroupCountX, usize threadGroupCountY, usize threadGroupCountZ) const
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

double GraphicsContext::GetMostRecentGpuTime() const
{
	return Backend->MostRecentGpuTime;
}

}
