#include "GraphicsContext.hpp"
#include "GpuDevice.hpp"
#include "PrivateCommon.hpp"

static constexpr usize FrameTimeQueryCount = 2;

GraphicsContext::GraphicsContext(GpuDevice* device)
	: MostRecentGpuTime(0.0)
	, CurrentGraphicsPipeline(nullptr)
	, Device(device)
{
	static constexpr D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	for (ID3D12CommandAllocator*& allocator : CommandAllocators)
	{
		CHECK_RESULT(Device->GetDevice()->CreateCommandAllocator(type, IID_PPV_ARGS(&allocator)));
	}
	CHECK_RESULT(Device->GetDevice()->CreateCommandList1(0, type, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&CommandList)));

#if !RELEASE
	constexpr D3D12_QUERY_HEAP_DESC frameTimeQueryHeapDescription =
	{
		.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP,
		.Count = FrameTimeQueryCount,
		.NodeMask = 0,
	};
	CHECK_RESULT(Device->GetDevice()->CreateQueryHeap(&frameTimeQueryHeapDescription, IID_PPV_ARGS(&FrameTimeQueryHeap)));

	const StringView frameTimeQueryHeapName = "Frame Time Query Heap"_view;
	SET_D3D_NAME(FrameTimeQueryHeap, frameTimeQueryHeapName);

	constexpr D3D12_RESOURCE_DESC1 frameTimeQueryResourceDescription =
	{
		.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
		.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
		.Width = (FramesInFlight + 1) * FrameTimeQueryCount * sizeof(uint64),
		.Height = 1,
		.DepthOrArraySize = 1,
		.MipLevels = 1,
		.Format = DXGI_FORMAT_UNKNOWN,
		.SampleDesc = DefaultSampleDescription,
		.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
		.Flags = D3D12_RESOURCE_FLAG_NONE,
		.SamplerFeedbackMipRegion = {},
	};
	CHECK_RESULT(Device->GetDevice()->CreateCommittedResource3(&ReadbackHeapProperties, D3D12_HEAP_FLAG_CREATE_NOT_ZEROED,
															   &frameTimeQueryResourceDescription, ToD3D12(BarrierLayout::Undefined), nullptr, nullptr,
															   0, NoCastableFormats, IID_PPV_ARGS(&FrameTimeQueryResource)));

	const StringView frameTimeQueryResourceName = "Frame Time Query Resource"_view;
	SET_D3D_NAME(FrameTimeQueryResource, frameTimeQueryResourceName);
#endif
}

GraphicsContext::~GraphicsContext()
{
#if !RELEASE
	Device->AddPendingDelete(FrameTimeQueryResource);
	Device->AddPendingDelete(FrameTimeQueryHeap);
#endif
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
	const usize backBufferIndex = Device->GetFrameIndex();

	CHECK_RESULT(CommandAllocators[backBufferIndex]->Reset());
	CHECK_RESULT(CommandList->Reset(CommandAllocators[backBufferIndex], nullptr));

#if !RELEASE
	CommandList->EndQuery(FrameTimeQueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, 0);
#endif

	ID3D12DescriptorHeap* heaps[] =
	{
		Device->ConstantBufferShaderResourceUnorderedAccessViewHeap.GetHeap(),
		Device->SamplerViewHeap.GetHeap(),
	};
	CommandList->SetDescriptorHeaps(ARRAY_COUNT(heaps), heaps);

	CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void GraphicsContext::End()
{
#if !RELEASE
	CommandList->EndQuery(FrameTimeQueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, 1);

	static uint32 currentFrameResolveIndex = 0;
	const uint64 currentFrameResolveOffset = currentFrameResolveIndex * FrameTimeQueryCount * sizeof(uint64);
	CommandList->ResolveQueryData(FrameTimeQueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, 0, FrameTimeQueryCount,
								  FrameTimeQueryResource, currentFrameResolveOffset);
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

	uint64* data;
	CHECK_RESULT(FrameTimeQueryResource->Map(0, &dataRange, reinterpret_cast<void**>(&data)));

	uint64 times[2] = {};
	Platform::MemoryCopy(times, data, FrameTimeQueryCount * sizeof(uint64));

	FrameTimeQueryResource->Unmap(0, &WriteNothing);

	const uint64 start = times[0];
	const uint64 end = times[1];
	MostRecentGpuTime = (start < end) ? (static_cast<double>(end - start) / Device->GetTimestampFrequency()) : 0.0;
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

void GraphicsContext::SetRenderTarget(const Texture& renderTarget) const
{
	const uint32 heapIndex = Device->Textures[renderTarget].GetHeapIndex();
	const D3D12_CPU_DESCRIPTOR_HANDLE cpu = { Device->RenderTargetViewHeap.GetCpu(heapIndex) };

	CommandList->OMSetRenderTargets(1, &cpu, true, nullptr);
}

void GraphicsContext::SetRenderTarget(const Texture& renderTarget, const Texture& depthStencil) const
{
	const uint32 renderTargetHeapIndex = Device->Textures[renderTarget].GetHeapIndex();
	const D3D12_CPU_DESCRIPTOR_HANDLE renderTargetCpu = { Device->RenderTargetViewHeap.GetCpu(renderTargetHeapIndex) };
	const uint32 depthStencilHeapIndex = Device->Textures[depthStencil].GetHeapIndex();
	const D3D12_CPU_DESCRIPTOR_HANDLE depthStencilCpu = { Device->DepthStencilViewHeap.GetCpu(depthStencilHeapIndex) };
	CommandList->OMSetRenderTargets(1, &renderTargetCpu, true, &depthStencilCpu);
}

void GraphicsContext::SetDepthRenderTarget(const Texture& depthStencil) const
{
	const uint32 heapIndex = Device->Textures[depthStencil].GetHeapIndex();
	const D3D12_CPU_DESCRIPTOR_HANDLE cpu = { Device->DepthStencilViewHeap.GetCpu(heapIndex) };
	CommandList->OMSetRenderTargets(0, nullptr, true, &cpu);
}

void GraphicsContext::ClearRenderTarget(const Texture& renderTarget, Float4 color) const
{
	const uint32 heapIndex = Device->Textures[renderTarget].GetHeapIndex();
	const D3D12_CPU_DESCRIPTOR_HANDLE cpu = { Device->RenderTargetViewHeap.GetCpu(heapIndex) };
	CommandList->ClearRenderTargetView(cpu, reinterpret_cast<const float*>(&color), 0, nullptr);
}

void GraphicsContext::ClearDepthStencil(const Texture& depthStencil) const
{
	const uint32 heapIndex = Device->Textures[depthStencil].GetHeapIndex();
	const D3D12_CPU_DESCRIPTOR_HANDLE cpu = { Device->DepthStencilViewHeap.GetCpu(heapIndex) };

	const D3D12_CLEAR_FLAGS clearFlag = IsStencilFormat(depthStencil.GetFormat()) ? (D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL) : D3D12_CLEAR_FLAG_DEPTH;
	CommandList->ClearDepthStencilView(cpu, clearFlag, D3D12_MAX_DEPTH, 0, 0, nullptr);
}

void GraphicsContext::SetGraphicsPipeline(GraphicsPipeline* graphicsPipeline)
{
	const D3D12GraphicsPipeline& apiGraphicsPipeline = Device->GraphicsPipelines[*graphicsPipeline];
	CurrentGraphicsPipeline = graphicsPipeline;

	CommandList->SetPipelineState(apiGraphicsPipeline.PipelineState);
	CommandList->SetGraphicsRootSignature(apiGraphicsPipeline.RootSignature);
}

void GraphicsContext::SetVertexBuffer(const Buffer& vertexBuffer, usize slot) const
{
	CHECK(vertexBuffer.GetType() == BufferType::VertexBuffer);

	const BufferResource resource = Device->Buffers[vertexBuffer].GetBufferResource(Device->GetFrameIndex(), vertexBuffer.IsStream());

	const D3D12_VERTEX_BUFFER_VIEW view =
	{
		.BufferLocation = resource->GetGPUVirtualAddress(),
		.SizeInBytes = static_cast<uint32>(vertexBuffer.GetSize()),
		.StrideInBytes = static_cast<uint32>(vertexBuffer.GetStride()),
	};
	CommandList->IASetVertexBuffers(static_cast<uint32>(slot), 1, &view);
}

void GraphicsContext::SetVertexBuffer(const Buffer& vertexBuffer, usize slot, usize offset, usize size, usize stride) const
{
	CHECK(vertexBuffer.GetType() == BufferType::VertexBuffer);

	const BufferResource resource = Device->Buffers[vertexBuffer].GetBufferResource(Device->GetFrameIndex(), vertexBuffer.IsStream());

	const D3D12_VERTEX_BUFFER_VIEW view =
	{
		.BufferLocation = resource->GetGPUVirtualAddress() + offset,
		.SizeInBytes = static_cast<uint32>(size),
		.StrideInBytes = static_cast<uint32>(stride),
	};
	CommandList->IASetVertexBuffers(static_cast<uint32>(slot), 1, &view);
}

void GraphicsContext::SetIndexBuffer(const Buffer& indexBuffer) const
{
	CHECK(indexBuffer.GetType() == BufferType::VertexBuffer);

	const BufferResource resource = Device->Buffers[indexBuffer].GetBufferResource(Device->GetFrameIndex(), indexBuffer.IsStream());

	CHECK(indexBuffer.GetStride() == 2 || indexBuffer.GetStride() == 4);
	const D3D12_INDEX_BUFFER_VIEW view =
	{
		.BufferLocation = resource->GetGPUVirtualAddress(),
		.SizeInBytes = static_cast<uint32>(indexBuffer.GetSize()),
		.Format = indexBuffer.GetStride() == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT,
	};
	CommandList->IASetIndexBuffer(&view);
}

void GraphicsContext::SetIndexBuffer(const Buffer& indexBuffer, usize offset, usize size, usize stride) const
{
	CHECK(indexBuffer.GetType() == BufferType::VertexBuffer);

	const BufferResource resource = Device->Buffers[indexBuffer].GetBufferResource(Device->GetFrameIndex(), indexBuffer.IsStream());

	CHECK(stride == 2 || stride == 4);
	const D3D12_INDEX_BUFFER_VIEW view =
	{
		.BufferLocation = resource->GetGPUVirtualAddress() + offset,
		.SizeInBytes = static_cast<uint32>(size),
		.Format = stride == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT,
	};
	CommandList->IASetIndexBuffer(&view);
}

void GraphicsContext::SetConstantBuffer(StringView name, const Buffer& constantBuffer, usize offsetIndex) const
{
	CHECK(CurrentGraphicsPipeline);
	CHECK(constantBuffer.GetType() == BufferType::ConstantBuffer);

	const D3D12GraphicsPipeline& currentPipeline = Device->GraphicsPipelines[*CurrentGraphicsPipeline];
	CHECK(currentPipeline.RootParameters.Contains(name));

	CHECK(offsetIndex < constantBuffer.GetCount());

	const usize offset = offsetIndex * constantBuffer.GetStride();
	const usize gpuAddress = Device->Buffers[constantBuffer].GetBufferResource(Device->GetFrameIndex(), constantBuffer.IsStream())->GetGPUVirtualAddress() + offset;
	CommandList->SetGraphicsRootConstantBufferView
	(
		static_cast<uint32>(currentPipeline.RootParameters[name].Index),
		D3D12_GPU_VIRTUAL_ADDRESS { gpuAddress }
	);
}

void GraphicsContext::SetRootConstants(StringView name, const void* data) const
{
	CHECK(CurrentGraphicsPipeline);

	const D3D12GraphicsPipeline& currentPipeline = Device->GraphicsPipelines[*CurrentGraphicsPipeline];
	CHECK(currentPipeline.RootParameters.Contains(name));

	const RootParameter& rootParameter = currentPipeline.RootParameters[name];

	CommandList->SetGraphicsRoot32BitConstants
	(
		static_cast<uint32>(rootParameter.Index),
		static_cast<uint32>(rootParameter.Size / sizeof(uint32)),
		data,
		0
	);
}

void GraphicsContext::Draw(usize vertexCount) const
{
	CHECK(CurrentGraphicsPipeline);
	CommandList->DrawInstanced(static_cast<uint32>(vertexCount), 1, 0, 0);
}

void GraphicsContext::DrawIndexed(usize indexCount) const
{
	CHECK(CurrentGraphicsPipeline);
	CommandList->DrawIndexedInstanced(static_cast<uint32>(indexCount), 1, 0, 0, 0);
}

void GraphicsContext::GlobalBarrier(BarrierPair<BarrierStage> stage, BarrierPair<BarrierAccess> access) const
{
	const D3D12_GLOBAL_BARRIER globalBarrier =
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
		.pGlobalBarriers = &globalBarrier,
	};
	CommandList->Barrier(1, &barrierGroup);
}

void GraphicsContext::BufferBarrier(BarrierPair<BarrierStage> stage, BarrierPair<BarrierAccess> access, const Buffer& buffer) const
{
	const D3D12_BUFFER_BARRIER bufferBarrier =
	{
		.SyncBefore = ToD3D12(stage.Before),
		.SyncAfter = ToD3D12(stage.After),
		.AccessBefore = ToD3D12(access.Before),
		.AccessAfter = ToD3D12(access.After),
		.pResource = Device->Buffers[buffer].GetBufferResource(Device->GetFrameIndex(), buffer.IsStream()),
		.Offset = 0,
		.Size = buffer.GetSize(),
	};
	const D3D12_BARRIER_GROUP barrierGroup =
	{
		.Type = D3D12_BARRIER_TYPE_BUFFER,
		.NumBarriers = 1,
		.pBufferBarriers = &bufferBarrier,
	};
	CommandList->Barrier(1, &barrierGroup);
}

void GraphicsContext::TextureBarrier(BarrierPair<BarrierStage> stage, BarrierPair<BarrierAccess> access, BarrierPair<BarrierLayout> layout, const Texture& texture) const
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
		.SyncBefore = ToD3D12(stage.Before),
		.SyncAfter = ToD3D12(stage.After),
		.AccessBefore = ToD3D12(access.Before),
		.AccessAfter = ToD3D12(access.After),
		.LayoutBefore = ToD3D12(layout.Before),
		.LayoutAfter = ToD3D12(layout.After),
		.pResource = Device->Textures[texture].GetTextureResource(),
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
