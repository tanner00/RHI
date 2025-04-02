#pragma once

#include "Common.hpp"

#include "Luft/Array.hpp"
#include "Luft/Hash.hpp"

enum class BufferType
{
	ConstantBuffer,
	VertexBuffer,
	StructuredBuffer,
	StorageBuffer,
};

enum class BufferUsage
{
	Static,
	Dynamic,
	Stream,
};

struct BufferDescription
{
	BufferType Type;
	BufferUsage Usage;
	usize Size;
	usize Stride;
};

class Buffer final : public RhiHandle<Buffer>
{
public:
	Buffer()
		: RhiHandle(0)
		, Description()
	{
	}

	Buffer(const RhiHandle& handle, BufferDescription&& description)
		: RhiHandle(handle)
		, Description(Move(description))
	{
	}

	usize GetSize() const { return Description.Size; }
	usize GetStride() const { return Description.Stride ? Description.Stride : Description.Size; }
	usize GetCount() const { return Description.Size / GetStride(); }

	BufferType GetType() const { return Description.Type; }

	bool IsStatic() const { return Description.Usage == BufferUsage::Static; }
	bool IsDynamic() const { return Description.Usage == BufferUsage::Dynamic; }
	bool IsStream() const { return Description.Usage == BufferUsage::Stream; }

private:
	BufferDescription Description;
};

HASH_RHI_HANDLE(Buffer);

BufferResource AllocateBuffer(ID3D12Device11* device, usize size, bool upload, bool allowUnorderedAccess, StringView name);

class D3D12Buffer
{
public:
	D3D12Buffer(ID3D12Device11* device,
				ViewHeap* constantBufferShaderResourceUnorderedAccessViewHeap,
				const Buffer& buffer,
				StringView name);
	D3D12Buffer(ID3D12Device11* device,
				ViewHeap* constantBufferShaderResourceUnorderedAccessViewHeap,
				const Buffer& buffer,
				const void* staticData,
				Array<UploadPair<Buffer>>* pendingBufferUploads,
				StringView name);

	BufferResource GetOnlyBufferResource() const
	{
		CHECK(Resource[0] && !Resource[1]);
		return Resource[0];
	}

	BufferResource GetBufferResource(usize index, bool stream) const
	{
		CHECK(index < FramesInFlight);
		const BufferResource current = stream ? Resource[index] : Resource[0];
		CHECK(current);
		return current;
	}

	uint32 GetOnlyHeapIndex() const
	{
		CHECK(HeapIndices[0] && !HeapIndices[1]);
		return HeapIndices[0];
	}

	uint32 GetHeapIndex(usize index, bool stream) const
	{
		CHECK(index < FramesInFlight);
		const uint32 heapIndex = stream ? HeapIndices[index] : HeapIndices[0];
		CHECK(heapIndex);
		return heapIndex;
	}

	void Write(ID3D12Device11* device,
			   const Buffer& buffer,
			   const void* data,
			   usize frameIndex,
			   Array<UploadPair<Buffer>>* pendingBufferUploads) const;

	BufferResource Resource[FramesInFlight];
	uint32 HeapIndices[FramesInFlight];
};
