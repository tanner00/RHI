#pragma once

#include "Common.hpp"

#include "Luft/Base.hpp"
#include "Luft/NoCopy.hpp"
#include "Luft/String.hpp"

enum class BarrierStage : uint32
{
	None = 0x0,
	All = 0x1,
	Draw = 0x2,
	InputAssembler = 0x4,
	VertexShading = 0x8,
	PixelShading = 0x10,
	DepthStencil = 0x20,
	RenderTarget = 0x40,
	ComputeShading = 0x80,
	RayTracing = 0x100,
	Copy = 0x200,
	Resolve = 0x400,
	ExecuteIndirect = 0x800,
	Predication = 0x800,
	AllShading = 0x1000,
	NonPixelShading = 0x2000,
};
inline BarrierStage operator|(BarrierStage a, BarrierStage b) { return static_cast<BarrierStage>(static_cast<uint32>(a) | static_cast<uint32>(b)); }

enum class BarrierAccess : uint32
{
	Common = 0,
	VertexBuffer = 0x1,
	ConstantBuffer = 0x2,
	IndexBuffer = 0x4,
	RenderTarget = 0x8,
	UnorderedAccess = 0x10,
	DepthStencilWrite = 0x20,
	DepthStencilRead = 0x40,
	ShaderResource = 0x80,
	StreamOutput = 0x100,
	IndirectArgument = 0x200,
	Predication = 0x200,
	CopyDestination = 0x400,
	CopySource = 0x800,
	ResolveDestination = 0x1000,
	ResolveSource = 0x2000,
	NoAccess = 0x80000000,
};
inline BarrierAccess operator|(BarrierAccess a, BarrierAccess b) { return static_cast<BarrierAccess>(static_cast<uint32>(a) | static_cast<uint32>(b)); }

enum class BarrierLayout : uint32
{
	Common = 0,
	Present = 0,
	GenericRead = 1,
	RenderTarget = 2,
	UnorderedAccess = 3,
	DepthStencilWrite = 4,
	DepthStencilRead = 5,
	ShaderResource = 6,
	CopySource = 7,
	CopyDestination = 8,
	ResolveSource = 9,
	ResolveDestination = 10,

	GraphicsQueueCommon = 18,
	GraphicsQueueGenericRead = 19,
	GraphicsQueueUnorderedAccess = 20,
	GraphicsQueueShaderResource = 21,
	GraphicsQueueCopySource = 22,
	GraphicsQueueCopyDestination = 23,

	ComputeQueueCommon = 24,
	ComputeQueueGenericRead = 25,
	ComputeQueueUnorderedAccess = 26,
	ComputeQueueShaderResource = 27,
	ComputeQueueCopySource = 28,
	ComputeQueueCopyDestination = 29,

	Undefined = 0xFFFFFFFF,
};

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

	void SetRenderTarget(const TextureHandle& handle) const;
	void SetRenderTarget(const TextureHandle& handle, const TextureHandle& depthStencilHandle) const;
	void SetDepthRenderTarget(const TextureHandle& handle) const;

	void ClearRenderTarget(const TextureHandle& handle, Float4 color) const;
	void ClearDepthStencil(const TextureHandle& handle) const;

	void SetGraphicsPipeline(GraphicsPipelineHandle* handle);

	void SetVertexBuffer(const BufferHandle& handle, usize slot) const;

	void SetConstantBuffer(StringView name, const BufferHandle& handle, usize offsetIndex = 0) const;
	void SetBuffer(StringView name, const BufferHandle& handle) const;

	void SetTexture(StringView name, const TextureHandle& handle) const;
	void SetSampler(StringView name, const SamplerHandle& handle) const;

	void Draw(usize vertexCount) const;

	void GlobalBarrier(BarrierPair<BarrierStage> stage, BarrierPair<BarrierAccess> access) const;
	void BufferBarrier(BarrierPair<BarrierStage> stage, BarrierPair<BarrierAccess> access, const BufferHandle& buffer) const;
	void TextureBarrier(BarrierPair<BarrierStage> stage, BarrierPair<BarrierAccess> access, BarrierPair<BarrierLayout> layout, const TextureHandle& handle) const;

private:
	ID3D12GraphicsCommandList10* GetCommandList() const;

	ID3D12CommandAllocator* CommandAllocators[FramesInFlight];
	ID3D12GraphicsCommandList10* CommandList;

	GraphicsPipelineHandle* CurrentGraphicsPipeline;

	GpuDevice* Device;

	friend GpuDevice;
};
