#include "GraphicsContext.hpp"
#include "BarrierConversion.hpp"
#include "GpuDevice.hpp"

#include "D3D12/d3d12.h"
#include <dxgi1_6.h>

GraphicsContext::GraphicsContext(GpuDevice* device)
	: CurrentGraphicsPipeline(nullptr)
	, Device(device)
{
	static constexpr D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	for (ID3D12CommandAllocator*& allocator : CommandAllocators)
	{
		CHECK_RESULT(Device->GetDevice()->CreateCommandAllocator(type, IID_PPV_ARGS(&allocator)));
	}
	CHECK_RESULT(Device->GetDevice()->CreateCommandList1(0, type, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&CommandList)));
}

GraphicsContext::~GraphicsContext()
{
	Device->AddPendingDelete(CommandList);
	CommandList = nullptr;

	for (ID3D12CommandAllocator*& commandAllocator : CommandAllocators)
	{
		Device->AddPendingDelete(commandAllocator);
		commandAllocator = nullptr;
	}

	CurrentGraphicsPipeline = nullptr;
}

void GraphicsContext::Begin() const
{
	const uint32 backBufferIndex = Device->GetSwapChain()->GetCurrentBackBufferIndex();

	CHECK_RESULT(CommandAllocators[backBufferIndex]->Reset());
	CHECK_RESULT(CommandList->Reset(CommandAllocators[backBufferIndex], nullptr));

	ID3D12DescriptorHeap* heaps[] =
	{
		Device->ConstantBufferShaderResourceUnorderedAccessViewHeap.GetHeap(),
		Device->SamplerViewHeap.GetHeap(),
	};
	CommandList->SetDescriptorHeaps(ARRAY_COUNT(heaps), heaps);

	CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void GraphicsContext::End() const
{
	CHECK_RESULT(CommandList->Close());
}

void GraphicsContext::SetViewport(uint32 width, uint32 height) const
{
	const D3D12_VIEWPORT viewport =
	{
		0.0f,
		0.0f,
		static_cast<float>(width),
		static_cast<float>(height),
		0.0f,
		1.0f,
	};
	const D3D12_RECT rect =
	{
		0,
		0,
		static_cast<int32>(width),
		static_cast<int32>(height),
	};

	CommandList->RSSetViewports(1, &viewport);
	CommandList->RSSetScissorRects(1, &rect);
}

void GraphicsContext::SetRenderTarget(const TextureHandle& handle) const
{
	Device->EnsureRenderTargetView(handle);

	const usize heapIndex = Device->Textures[handle.Get()].HeapIndices[static_cast<usize>(ViewType::RenderTarget)];
	const D3D12_CPU_DESCRIPTOR_HANDLE cpu = { Device->RenderTargetViewHeap.GetCpu(heapIndex) };

	CommandList->OMSetRenderTargets(1, &cpu, true, nullptr);
}

void GraphicsContext::SetRenderTarget(const TextureHandle& handle, const TextureHandle& depthStencilHandle) const
{
	Device->EnsureRenderTargetView(handle);
	Device->EnsureDepthStencilView(depthStencilHandle);

	const usize renderTargetHeapIndex = Device->Textures[handle.Get()].HeapIndices[static_cast<usize>(ViewType::RenderTarget)];
	const D3D12_CPU_DESCRIPTOR_HANDLE renderTargetCpu = { Device->RenderTargetViewHeap.GetCpu(renderTargetHeapIndex) };
	const usize depthStencilHeapIndex = Device->Textures[depthStencilHandle.Get()].HeapIndices[static_cast<usize>(ViewType::DepthStencil)];
	const D3D12_CPU_DESCRIPTOR_HANDLE depthStencilCpu = { Device->DepthStencilViewHeap.GetCpu(depthStencilHeapIndex) };

	CommandList->OMSetRenderTargets(1, &renderTargetCpu, true, &depthStencilCpu);
}

void GraphicsContext::SetDepthRenderTarget(const TextureHandle& handle) const
{
	Device->EnsureDepthStencilView(handle);

	const usize heapIndex = Device->Textures[handle.Get()].HeapIndices[static_cast<usize>(ViewType::DepthStencil)];
	const D3D12_CPU_DESCRIPTOR_HANDLE cpu = { Device->DepthStencilViewHeap.GetCpu(heapIndex) };
	CommandList->OMSetRenderTargets(0, nullptr, true, &cpu);
}

void GraphicsContext::ClearRenderTarget(const TextureHandle& handle, Float4 color) const
{
	Device->EnsureRenderTargetView(handle);

	const usize heapIndex = Device->Textures[handle.Get()].HeapIndices[static_cast<usize>(ViewType::RenderTarget)];
	const D3D12_CPU_DESCRIPTOR_HANDLE cpu = { Device->RenderTargetViewHeap.GetCpu(heapIndex) };

	CommandList->ClearRenderTargetView(cpu, reinterpret_cast<const float*>(&color), 0, nullptr);
}

void GraphicsContext::ClearDepthStencil(const TextureHandle& handle) const
{
	Device->EnsureDepthStencilView(handle);

	const usize heapIndex = Device->Textures[handle.Get()].HeapIndices[static_cast<usize>(ViewType::DepthStencil)];
	const D3D12_CPU_DESCRIPTOR_HANDLE cpu = { Device->DepthStencilViewHeap.GetCpu(heapIndex) };

	const D3D12_CLEAR_FLAGS clearFlag = IsStencilFormat(handle.GetFormat()) ? (D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL) : D3D12_CLEAR_FLAG_DEPTH;
	CommandList->ClearDepthStencilView(cpu, clearFlag, D3D12_MAX_DEPTH, 0, 0, nullptr);
}

void GraphicsContext::SetGraphicsPipeline(GraphicsPipelineHandle& pipeline)
{
	const GraphicsPipeline& graphicsPipeline = Device->GraphicsPipelines[pipeline.Get()];
	CurrentGraphicsPipeline = &pipeline;

	CommandList->SetPipelineState(graphicsPipeline.PipelineState);
	CommandList->SetGraphicsRootSignature(graphicsPipeline.RootSignature);
}

void GraphicsContext::SetVertexBuffer(const BufferHandle& handle, usize slot) const
{
	CHECK(handle.GetType() == BufferType::VertexBuffer);

	const BufferResource resource = Device->Buffers[handle.Get()].GetBufferResource(Device->GetFrameIndex(), handle.IsStream());

	const D3D12_VERTEX_BUFFER_VIEW view =
	{
		.BufferLocation = resource->GetGPUVirtualAddress(),
		.SizeInBytes = static_cast<uint32>(handle.GetSize()),
		.StrideInBytes = static_cast<uint32>(handle.GetStride()),
	};
	CommandList->IASetVertexBuffers(static_cast<uint32>(slot), 1, &view);
}

void GraphicsContext::SetConstantBuffer(StringView name, const BufferHandle& handle, usize offsetIndex) const
{
	CHECK(CurrentGraphicsPipeline);

	CHECK(handle.GetType() == BufferType::ConstantBuffer);

	const GraphicsPipeline& currentPipeline = Device->GraphicsPipelines[CurrentGraphicsPipeline->Get()];
	CHECK(currentPipeline.Parameters.Contains(name));

	CHECK(offsetIndex < handle.GetCount());

	Device->EnsureConstantBufferView(handle);

	const usize offset = offsetIndex * handle.GetStride();
	const usize gpuAddress = Device->Buffers[handle.Get()].GetBufferResource(Device->GetFrameIndex(), handle.IsStream())->GetGPUVirtualAddress() + offset;
	CommandList->SetGraphicsRootConstantBufferView
	(
		static_cast<uint32>(currentPipeline.Parameters[name]),
		D3D12_GPU_VIRTUAL_ADDRESS { gpuAddress }
	);
}

void GraphicsContext::SetBuffer(StringView name, const BufferHandle& handle) const
{
	CHECK(CurrentGraphicsPipeline);

	CHECK(handle.GetType() == BufferType::StructuredBuffer);

	const GraphicsPipeline& currentPipeline = Device->GraphicsPipelines[CurrentGraphicsPipeline->Get()];
	CHECK(currentPipeline.Parameters.Contains(name));

	Device->EnsureShaderResourceView(handle);

	const usize gpuAddress = Device->Buffers[handle.Get()].GetBufferResource(Device->GetFrameIndex(), handle.IsStream())->GetGPUVirtualAddress();
	CommandList->SetGraphicsRootShaderResourceView
	(
		static_cast<uint32>(currentPipeline.Parameters[name]),
		D3D12_GPU_VIRTUAL_ADDRESS { gpuAddress }
	);
}

void GraphicsContext::SetTexture(StringView name, const TextureHandle& handle) const
{
	CHECK(CurrentGraphicsPipeline);

	const GraphicsPipeline& currentPipeline = Device->GraphicsPipelines[CurrentGraphicsPipeline->Get()];
	CHECK(currentPipeline.Parameters.Contains(name));

	Device->EnsureShaderResourceView(handle);

	const usize textureHeapIndex = Device->Textures[handle.Get()].HeapIndices[static_cast<usize>(ViewType::ShaderResource)];
	const usize gpu = Device->ConstantBufferShaderResourceUnorderedAccessViewHeap.GetGpu(textureHeapIndex);
	CommandList->SetGraphicsRootDescriptorTable
	(
		static_cast<uint32>(currentPipeline.Parameters[name]),
		D3D12_GPU_DESCRIPTOR_HANDLE { gpu }
	);
}

void GraphicsContext::SetSampler(StringView name, const SamplerHandle& handle) const
{
	CHECK(CurrentGraphicsPipeline);

	const GraphicsPipeline& currentPipeline = Device->GraphicsPipelines[CurrentGraphicsPipeline->Get()];
	CHECK(currentPipeline.Parameters.Contains(name));

	const usize heapIndex = Device->Samplers[handle.Get()].HeapIndex;
	const usize gpu = Device->SamplerViewHeap.GetGpu(heapIndex);
	CommandList->SetGraphicsRootDescriptorTable
	(
		static_cast<uint32>(currentPipeline.Parameters[name]),
		D3D12_GPU_DESCRIPTOR_HANDLE { gpu }
	);
}

void GraphicsContext::Draw(usize vertexCount) const
{
	CHECK(CurrentGraphicsPipeline);
	CommandList->DrawInstanced(static_cast<uint32>(vertexCount), 1, 0, 0);
}

void GraphicsContext::GlobalBarrier(BarrierPair<BarrierStage> stage, BarrierPair<BarrierAccess> access) const
{
	const D3D12_GLOBAL_BARRIER d3d12Barrier =
	{
		.SyncBefore = ToD3D12(stage.Before),
		.SyncAfter = ToD3D12(stage.After),
		.AccessBefore = ToD3D12(access.Before),
		.AccessAfter = ToD3D12(access.After),
	};
	const D3D12_BARRIER_GROUP barrierGroup =
	{
		.Type = D3D12_BARRIER_TYPE_GLOBAL,
		.NumBarriers = 1,
		.pGlobalBarriers = &d3d12Barrier,
	};
	CommandList->Barrier(1, &barrierGroup);
}

void GraphicsContext::BufferBarrier(BarrierPair<BarrierStage> stage, BarrierPair<BarrierAccess> access, const BufferHandle& buffer) const
{
	const D3D12_BUFFER_BARRIER d3d12Barrier =
	{
		.SyncBefore = ToD3D12(stage.Before),
		.SyncAfter = ToD3D12(stage.After),
		.AccessBefore = ToD3D12(access.Before),
		.AccessAfter = ToD3D12(access.After),
		.pResource = Device->Buffers[buffer.Get()].GetBufferResource(Device->GetFrameIndex(), buffer.IsStream()),
		.Offset = 0,
		.Size = buffer.GetSize(),
	};
	const D3D12_BARRIER_GROUP barrierGroup =
	{
		.Type = D3D12_BARRIER_TYPE_BUFFER,
		.NumBarriers = 1,
		.pBufferBarriers = &d3d12Barrier,
	};
	CommandList->Barrier(1, &barrierGroup);
}

void GraphicsContext::TextureBarrier(BarrierPair<BarrierStage> stage, BarrierPair<BarrierAccess> access, BarrierPair<BarrierLayout> layout, const TextureHandle& texture) const
{
	static constexpr D3D12_TEXTURE_BARRIER_FLAGS noDiscard = D3D12_TEXTURE_BARRIER_FLAG_NONE;
	static constexpr D3D12_BARRIER_SUBRESOURCE_RANGE entireRange =
	{
		.IndexOrFirstMipLevel = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
		.NumMipLevels = 0,
		.FirstArraySlice = 0,
		.NumArraySlices = 0,
		.FirstPlane = 0,
		.NumPlanes = 0,
	};
	const D3D12_TEXTURE_BARRIER d3d12Barrier =
	{
		.SyncBefore = ToD3D12(stage.Before),
		.SyncAfter = ToD3D12(stage.After),
		.AccessBefore = ToD3D12(access.Before),
		.AccessAfter = ToD3D12(access.After),
		.LayoutBefore = ToD3D12(layout.Before),
		.LayoutAfter = ToD3D12(layout.After),
		.pResource = Device->Textures[texture.Get()].GetTextureResource(),
		.Subresources = entireRange,
		.Flags = noDiscard,
	};
	const D3D12_BARRIER_GROUP barrierGroup =
	{
		.Type = D3D12_BARRIER_TYPE_TEXTURE,
		.NumBarriers = 1,
		.pTextureBarriers = &d3d12Barrier,
	};
	CommandList->Barrier(1, &barrierGroup);
}

ID3D12GraphicsCommandList10* GraphicsContext::GetCommandList() const
{
	CHECK(CommandList);
	return CommandList;
}

