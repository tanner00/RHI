#include "GraphicsContext.hpp"
#include "Base.hpp"
#include "ComputePipeline.hpp"
#include "Convert.hpp"
#include "Device.hpp"
#include "GraphicsPipeline.hpp"
#include "Pipeline.hpp"
#include "Resource.hpp"
#include "TextureView.hpp"

namespace RHI::D3D12
{

static constexpr usize FrameTimeQueryCount = 2;

GraphicsContext::GraphicsContext(const GraphicsContextDescription& description, D3D12::Device* device)
	: GraphicsContextDescription(description)
	, CurrentPipeline(nullptr)
	, Device(device)
	, MostRecentGpuTime(0.0)
{
	static constexpr D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	for (ID3D12CommandAllocator*& allocator : CommandAllocators)
	{
		CHECK_RESULT(Device->Native->CreateCommandAllocator(type, IID_PPV_ARGS(&allocator)));
	}
	CHECK_RESULT(Device->Native->CreateCommandList1(0, type, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&CommandList)));

#if !RELEASE
	static constexpr D3D12_QUERY_HEAP_DESC frameTimeQueryHeapDescription =
	{
		.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP,
		.Count = FrameTimeQueryCount,
		.NodeMask = 0,
	};
	CHECK_RESULT(Device->Native->CreateQueryHeap(&frameTimeQueryHeapDescription, IID_PPV_ARGS(&FrameTimeQueryHeap)));

	const StringView frameTimeQueryHeapName = "Frame Time Query Heap"_view;
	SET_D3D_NAME(FrameTimeQueryHeap, frameTimeQueryHeapName);

	FrameTimeQueryResource = Device->Create(
	{
		.Type = ResourceType::Buffer,
		.Format = ResourceFormat::None,
		.Flags = ResourceFlags::ReadBack,
		.InitialLayout = BarrierLayout::Undefined,
		.Size = (FramesInFlight + 1) * FrameTimeQueryCount * sizeof(uint64),
		.Name = "Frame Time Query Resource"_view,
	});
#endif
}

GraphicsContext::~GraphicsContext()
{
#if !RELEASE
	Device->Destroy(FrameTimeQueryResource);
	FrameTimeQueryResource = nullptr;
	Device->AddPendingDestroy(FrameTimeQueryHeap);
	FrameTimeQueryHeap = nullptr;
#endif

	Device->AddPendingDestroy(CommandList);
	CommandList = nullptr;

	for (ID3D12CommandAllocator*& commandAllocator : CommandAllocators)
	{
		Device->AddPendingDestroy(commandAllocator);
		commandAllocator = nullptr;
	}

	CurrentPipeline = nullptr;
}

void GraphicsContext::Begin() const
{
	const usize backBufferIndex = Device->GetFrameIndex();

	CHECK_RESULT(CommandAllocators[backBufferIndex]->Reset());
	CHECK_RESULT(CommandList->Reset(CommandAllocators[backBufferIndex], nullptr));

#if !RELEASE
	CommandList->EndQuery(FrameTimeQueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, 0);
#endif

	ID3D12DescriptorHeap* heaps[] =
	{
		Device->ConstantBufferShaderResourceUnorderedAccessViewHeap.Native,
		Device->SamplerViewHeap.Native,
	};
	CommandList->SetDescriptorHeaps(ARRAY_COUNT(heaps), heaps);

	CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void GraphicsContext::End()
{
#if !RELEASE
	ID3D12Resource2* frameTimeQueryResourceNative = FrameTimeQueryResource->Native;

	CommandList->EndQuery(FrameTimeQueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, 1);

	static uint32 currentFrameResolveIndex = 0;
	const uint64 currentFrameResolveOffset = currentFrameResolveIndex * FrameTimeQueryCount * sizeof(uint64);
	CommandList->ResolveQueryData(FrameTimeQueryHeap,
								  D3D12_QUERY_TYPE_TIMESTAMP,
								  0,
								  FrameTimeQueryCount,
								  frameTimeQueryResourceNative,
								  currentFrameResolveOffset);
#endif

	CHECK_RESULT(CommandList->Close());

#if !RELEASE
	const uint32 readbackFrameIndex = (currentFrameResolveIndex + 1) % (FramesInFlight + 1);
	const usize readbackOffset = readbackFrameIndex * FrameTimeQueryCount * sizeof(uint64);
	currentFrameResolveIndex = readbackFrameIndex;

	const D3D12_RANGE dataRange =
	{
		.Begin = readbackOffset,
		.End = readbackOffset + FrameTimeQueryCount * sizeof(uint64),
	};

	uint64* data = nullptr;
	CHECK_RESULT(frameTimeQueryResourceNative->Map(0, &dataRange, reinterpret_cast<void**>(&data)));
	uint64 times[FrameTimeQueryCount] = {};
	Platform::MemoryCopy(times, data, FrameTimeQueryCount * sizeof(uint64));
	frameTimeQueryResourceNative->Unmap(0, WriteEverything);

	const uint64 start = times[0];
	const uint64 end = times[1];
	MostRecentGpuTime = (start < end) ? (static_cast<double>(end - start) / Device->TimestampFrequency) : 0.0;
#endif
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

void GraphicsContext::SetRenderTarget(const TextureView* renderTarget) const
{
	const D3D12_CPU_DESCRIPTOR_HANDLE cpu = renderTarget->GetCpu();
	switch (renderTarget->Type)
	{
	case ViewType::RenderTarget:
		CommandList->OMSetRenderTargets(1, &cpu, false, nullptr);
		break;
	case ViewType::DepthStencil:
		CommandList->OMSetRenderTargets(0, nullptr, false, &cpu);
		break;
	default:
		CHECK(false);
	}
}

void GraphicsContext::SetRenderTarget(const TextureView* renderTarget, const TextureView* depthStencil) const
{
	const D3D12_CPU_DESCRIPTOR_HANDLE renderTargetCpu = renderTarget->GetCpu();
	const D3D12_CPU_DESCRIPTOR_HANDLE depthStencilCpu = depthStencil->GetCpu();
	CommandList->OMSetRenderTargets(1, &renderTargetCpu, false, &depthStencilCpu);
}

void GraphicsContext::SetRenderTargets(const ArrayView<const TextureView*>& renderTargets, const TextureView* depthStencil) const
{
	CHECK(renderTargets.GetLength() < MaxRenderTargetCount);

	D3D12_CPU_DESCRIPTOR_HANDLE renderTargetCpus[MaxRenderTargetCount] = {};
	for (usize renderTargetIndex = 0; renderTargetIndex < renderTargets.GetLength(); ++renderTargetIndex)
	{
		renderTargetCpus[renderTargetIndex] = renderTargets[renderTargetIndex]->GetCpu();
	}
	const D3D12_CPU_DESCRIPTOR_HANDLE depthStencilCpu = depthStencil->GetCpu();
	CommandList->OMSetRenderTargets(static_cast<uint32>(renderTargets.GetLength()), renderTargetCpus, false, &depthStencilCpu);
}

void GraphicsContext::SetDepthRenderTarget(const TextureView* depthStencil) const
{
	const D3D12_CPU_DESCRIPTOR_HANDLE cpu = depthStencil->GetCpu();
	CommandList->OMSetRenderTargets(1, nullptr, false, &cpu);
}

void GraphicsContext::ClearRenderTarget(const TextureView* renderTarget) const
{
	const D3D12_CPU_DESCRIPTOR_HANDLE cpu = renderTarget->GetCpu();
	CommandList->ClearRenderTargetView(cpu, reinterpret_cast<const float*>(&renderTarget->Resource.ColorClear), 0, nullptr);
}

void GraphicsContext::ClearDepthStencil(const TextureView* depthStencil) const
{
	const D3D12_CPU_DESCRIPTOR_HANDLE cpu = depthStencil->GetCpu();
	const D3D12_CLEAR_FLAGS clearFlags = IsStencilFormat(depthStencil->Resource.Format)
									   ? (D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL)
									   : D3D12_CLEAR_FLAG_DEPTH;
	CommandList->ClearDepthStencilView(cpu, clearFlags, depthStencil->Resource.DepthClear, 0, 0, nullptr);
}

void GraphicsContext::SetPipeline(GraphicsPipeline* pipeline)
{
	CommandList->SetGraphicsRootSignature(pipeline->RootSignature);
	CommandList->SetPipelineState(pipeline->PipelineState);
	CurrentPipeline = pipeline;
}

void GraphicsContext::SetPipeline(ComputePipeline* pipeline)
{
	CommandList->SetComputeRootSignature(pipeline->RootSignature);
	CommandList->SetPipelineState(pipeline->PipelineState);
	CurrentPipeline = pipeline;
}

void GraphicsContext::SetVertexBuffer(usize slot, const SubBuffer& vertexBuffer) const
{
	const D3D12_VERTEX_BUFFER_VIEW view =
	{
		.BufferLocation = vertexBuffer.Resource.Backend->Native->GetGPUVirtualAddress() + vertexBuffer.Offset,
		.SizeInBytes = static_cast<uint32>(vertexBuffer.Size),
		.StrideInBytes = static_cast<uint32>(vertexBuffer.Stride),
	};
	CommandList->IASetVertexBuffers(static_cast<uint32>(slot), 1, &view);
}

void GraphicsContext::SetIndexBuffer(const SubBuffer& indexBuffer) const
{
	CHECK(indexBuffer.Stride == sizeof(uint16) || indexBuffer.Stride == sizeof(uint32));
	const D3D12_INDEX_BUFFER_VIEW view =
	{
		.BufferLocation = indexBuffer.Resource.Backend->Native->GetGPUVirtualAddress() + indexBuffer.Offset,
		.SizeInBytes = static_cast<uint32>(indexBuffer.Size),
		.Format = indexBuffer.Stride == sizeof(uint16) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT,
	};
	CommandList->IASetIndexBuffer(&view);
}

void GraphicsContext::SetConstantBuffer(StringView name, const Resource* buffer, usize offset) const
{
	CHECK(CurrentPipeline);
	CurrentPipeline->SetConstantBuffer(CommandList, name, buffer, offset);
}

void GraphicsContext::SetRootConstants(const void* data) const
{
	CHECK(CurrentPipeline);
	CurrentPipeline->SetConstants(CommandList, data);
}

void GraphicsContext::Draw(usize vertexCount) const
{
	CommandList->DrawInstanced(static_cast<uint32>(vertexCount), 1, 0, 0);
}

void GraphicsContext::DrawIndexed(usize indexCount) const
{
	CommandList->DrawIndexedInstanced(static_cast<uint32>(indexCount), 1, 0, 0, 0);
}

void GraphicsContext::Dispatch(usize threadGroupCountX, usize threadGroupCountY, usize threadGroupCountZ) const
{
	CommandList->Dispatch(static_cast<uint32>(threadGroupCountX), static_cast<uint32>(threadGroupCountY), static_cast<uint32>(threadGroupCountZ));
}

void GraphicsContext::Copy(const Resource* destination, const Resource* source) const
{
	CommandList->CopyResource(destination->Native, source->Native);
}

void GraphicsContext::GlobalBarrier(BarrierPair<BarrierStage> stage, BarrierPair<BarrierAccess> access) const
{
	const D3D12_GLOBAL_BARRIER globalBarrier =
	{
		.SyncBefore = To(stage.Before),
		.SyncAfter = To(stage.After),
		.AccessBefore = To(access.Before),
		.AccessAfter = To(access.After),
	};
	const D3D12_BARRIER_GROUP barrierGroup =
	{
		.Type = D3D12_BARRIER_TYPE_GLOBAL,
		.NumBarriers = 1,
		.pGlobalBarriers = &globalBarrier,
	};
	CommandList->Barrier(1, &barrierGroup);
}

void GraphicsContext::BufferBarrier(BarrierPair<BarrierStage> stage, BarrierPair<BarrierAccess> access, const Resource* buffer) const
{
	const D3D12_BUFFER_BARRIER bufferBarrier =
	{
		.SyncBefore = To(stage.Before),
		.SyncAfter = To(stage.After),
		.AccessBefore = To(access.Before),
		.AccessAfter = To(access.After),
		.pResource = buffer->Native,
		.Offset = 0,
		.Size = buffer->Size,
	};
	const D3D12_BARRIER_GROUP barrierGroup =
	{
		.Type = D3D12_BARRIER_TYPE_BUFFER,
		.NumBarriers = 1,
		.pBufferBarriers = &bufferBarrier,
	};
	CommandList->Barrier(1, &barrierGroup);
}

void GraphicsContext::TextureBarrier(BarrierPair<BarrierStage> stage,
									 BarrierPair<BarrierAccess> access,
									 BarrierPair<BarrierLayout> layout,
									 const Resource* texture) const
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
	const D3D12_TEXTURE_BARRIER textureBarrier =
	{
		.SyncBefore = To(stage.Before),
		.SyncAfter = To(stage.After),
		.AccessBefore = To(access.Before),
		.AccessAfter = To(access.After),
		.LayoutBefore = To(layout.Before),
		.LayoutAfter = To(layout.After),
		.pResource = texture->Native,
		.Subresources = entireRange,
		.Flags = noDiscard,
	};
	const D3D12_BARRIER_GROUP barrierGroup =
	{
		.Type = D3D12_BARRIER_TYPE_TEXTURE,
		.NumBarriers = 1,
		.pTextureBarriers = &textureBarrier,
	};
	CommandList->Barrier(1, &barrierGroup);
}

void GraphicsContext::BuildAccelerationStructure(const AccelerationStructureGeometry& geometry,
												 const Resource* scratchResource,
												 const Resource* resultResource) const
{
	CHECK((resultResource->Native->GetGPUVirtualAddress() % D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT) == 0);

	const D3D12_RAYTRACING_GEOMETRY_DESC geometryDescription = To(geometry);
	const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = To(geometryDescription);

	const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC description =
	{
		.DestAccelerationStructureData = resultResource->Native->GetGPUVirtualAddress(),
		.Inputs = inputs,
		.SourceAccelerationStructureData = D3D12_GPU_VIRTUAL_ADDRESS { 0 },
		.ScratchAccelerationStructureData = scratchResource->Native->GetGPUVirtualAddress(),
	};
	CommandList->BuildRaytracingAccelerationStructure(&description, 0, NoPostBuildSizes);
}

void GraphicsContext::BuildAccelerationStructure(const Buffer& instances,
												 const Resource* scratchResource,
												 const Resource* resultResource) const
{
	CHECK((resultResource->Native->GetGPUVirtualAddress() % D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT) == 0);

	const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC description =
	{
		.DestAccelerationStructureData = resultResource->Native->GetGPUVirtualAddress(),
		.Inputs = To(instances),
		.SourceAccelerationStructureData = D3D12_GPU_VIRTUAL_ADDRESS { 0 },
		.ScratchAccelerationStructureData = scratchResource->Native->GetGPUVirtualAddress(),
	};
	CommandList->BuildRaytracingAccelerationStructure(&description, 0, NoPostBuildSizes);
}

void GraphicsContext::Execute(ID3D12CommandQueue* queue) const
{
	ID3D12CommandList* commands[] = { CommandList };
	queue->ExecuteCommandLists(ARRAY_COUNT(commands), commands);
}

}
