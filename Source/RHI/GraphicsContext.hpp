#pragma once

#include "Common.hpp"

#include "Luft/NoCopy.hpp"
#include "Luft/String.hpp"

template<typename T>
struct BarrierPair
{
	T Before;
	T After;
};

class GraphicsContext : public NoCopy
{
public:
	explicit GraphicsContext(GpuDevice* device);
	~GraphicsContext();

	void Begin() const;
	void End() const;

	void SetViewport(uint32 width, uint32 height) const;

	void SetRenderTarget(const Texture& renderTarget) const;
	void SetRenderTarget(const Texture& renderTarget, const Texture& depthStencil) const;
	void SetDepthRenderTarget(const Texture& depthStencil) const;

	void ClearRenderTarget(const Texture& renderTarget, Float4 color) const;
	void ClearDepthStencil(const Texture& depthStencil) const;

	void SetGraphicsPipeline(GraphicsPipeline* graphicsPipeline);

	void SetVertexBuffer(const Buffer& vertexBuffer, usize slot) const;
	void SetIndexBuffer(const Buffer& indexBuffer) const;

	void SetConstantBuffer(StringView name, const Buffer& constantBuffer, usize offsetIndex = 0) const;

	void Draw(usize vertexCount) const;
	void DrawIndexed(usize indexCount) const;

	void GlobalBarrier(BarrierPair<BarrierStage> stage, BarrierPair<BarrierAccess> access) const;
	void BufferBarrier(BarrierPair<BarrierStage> stage, BarrierPair<BarrierAccess> access, const Buffer& buffer) const;
	void TextureBarrier(BarrierPair<BarrierStage> stage, BarrierPair<BarrierAccess> access, BarrierPair<BarrierLayout> layout, const Texture& texture) const;

private:
	ID3D12GraphicsCommandList10* GetCommandList() const;

	ID3D12CommandAllocator* CommandAllocators[FramesInFlight];
	ID3D12GraphicsCommandList10* CommandList;

	GraphicsPipeline* CurrentGraphicsPipeline;

	GpuDevice* Device;

	friend GpuDevice;
};
