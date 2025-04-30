#pragma once

#include "RHI/Device.hpp"
#include "RHI/GraphicsContext.hpp"
#include "RHI/HLSL.hpp"

#include "Luft/String.hpp"

namespace RHI::D3D12
{

class GraphicsContext final : public GraphicsContextDescription, NoCopy
{
public:
	GraphicsContext(const GraphicsContextDescription& description, Device* device);
	~GraphicsContext();

	void Begin() const;
	void End();

	void SetViewport(uint32 width, uint32 height) const;

	void SetRenderTarget(const TextureView* renderTarget) const;
	void SetRenderTarget(const TextureView* renderTarget, const TextureView* depthStencil) const;
	void SetDepthRenderTarget(const TextureView* depthStencil) const;

	void ClearRenderTarget(const TextureView* renderTarget, Float4 color) const;
	void ClearDepthStencil(const TextureView* depthStencil) const;

	void SetPipeline(GraphicsPipeline* pipeline);
	void SetPipeline(ComputePipeline* pipeline);

	void SetVertexBuffer(usize slot, const SubBuffer& vertexBuffer) const;
	void SetIndexBuffer(const SubBuffer& indexBuffer) const;

	void SetConstantBuffer(StringView name, const Resource* buffer, usize offset = 0) const;

	void SetRootConstants(const void* data) const;

	void Draw(usize vertexCount) const;
	void DrawIndexed(usize indexCount) const;
	void Dispatch(usize threadGroupCountX, usize threadGroupCountY, usize threadGroupCountZ) const;

	void Copy(const Resource* destination, const Resource* source) const;

	void GlobalBarrier(BarrierPair<BarrierStage> stage, BarrierPair<BarrierAccess> access) const;
	void BufferBarrier(BarrierPair<BarrierStage> stage, BarrierPair<BarrierAccess> access, const Resource* buffer) const;
	void TextureBarrier(BarrierPair<BarrierStage> stage,
						BarrierPair<BarrierAccess> access,
						BarrierPair<BarrierLayout> layout,
						const Resource* texture) const;

	void BuildAccelerationStructure(const SubBuffer& vertexBuffer,
									const SubBuffer& indexBuffer,
									const Resource* scratchResource,
									const Resource* resultResource) const;
	void BuildAccelerationStructure(const SubBuffer& instancesBuffer,
									const Resource* scratchResource,
									const Resource* resultResource) const;

	void Execute(ID3D12CommandQueue* queue) const;

	ID3D12CommandAllocator* CommandAllocators[FramesInFlight];
	ID3D12GraphicsCommandList10* CommandList;

	Pipeline* CurrentPipeline;
	Device* Device;

#if !RELEASE
	ID3D12QueryHeap* FrameTimeQueryHeap;
	Resource* FrameTimeQueryResource;
#endif

	double MostRecentGpuTime;
};

}
