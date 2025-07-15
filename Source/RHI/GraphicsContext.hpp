#pragma once

#include "Barrier.hpp"
#include "Forward.hpp"
#include "HLSL.hpp"

#include "Luft/Array.hpp"
#include "Luft/String.hpp"

namespace RHI
{

template<typename T>
struct BarrierPair
{
	T Before;
	T After;
};

struct GraphicsContextDescription
{
};

class GraphicsContext final : public GraphicsContextDescription
{
public:
	GraphicsContext(const GraphicsContextDescription& description, RHI_BACKEND(GraphicsContext)* backend)
		: GraphicsContextDescription(description)
		, Backend(backend)
	{
	}

	void Begin() const;
	void End();

	void SetViewport(uint32 width, uint32 height) const;

	void SetRenderTarget(const TextureView& renderTarget) const;
	void SetRenderTarget(const TextureView& renderTarget, const TextureView& depthStencil) const;
	void SetRenderTargets(const ArrayView<const TextureView>& renderTargets, const TextureView& depthStencil) const;
	void SetDepthRenderTarget(const TextureView& depthStencil) const;

	void ClearRenderTarget(const TextureView& renderTarget) const;
	void ClearDepthStencil(const TextureView& depthStencil) const;

	void SetPipeline(const GraphicsPipeline& pipeline);
	void SetPipeline(const ComputePipeline& pipeline);

	void SetVertexBuffer(usize slot, const SubBuffer& vertexBuffer) const;
	void SetIndexBuffer(const SubBuffer& indexBuffer) const;

	void SetConstantBuffer(StringView name, const Resource& buffer, usize offset = 0) const;
	void SetRootConstants(const void* data) const;

	void Draw(usize vertexCount) const;
	void DrawIndexed(usize indexCount) const;
	void Dispatch(uint32 threadGroupCountX, uint32 threadGroupCountY, uint32 threadGroupCountZ) const;

	void Copy(const Resource& destination, const Resource& source) const;

	void GlobalBarrier(BarrierPair<BarrierStage> stage, BarrierPair<BarrierAccess> access) const;
	void BufferBarrier(BarrierPair<BarrierStage> stage, BarrierPair<BarrierAccess> access, const Resource& buffer) const;
	void TextureBarrier(BarrierPair<BarrierStage> stage,
						BarrierPair<BarrierAccess> access,
						BarrierPair<BarrierLayout> layout,
						const Resource& texture) const;

	void BuildAccelerationStructure(const AccelerationStructureGeometry& geometry,
									const Resource& scratchResource,
									const Resource& resultResource) const;
	void BuildAccelerationStructure(const Buffer& instances,
									const Resource& scratchResource,
									const Resource& resultResource) const;

	double GetMostRecentGpuTime() const;

	RHI_BACKEND(GraphicsContext)* Backend;
};

}
