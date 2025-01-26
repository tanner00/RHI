#pragma once

#include "Common.hpp"

#include "Luft/Array.hpp"
#include "Luft/Hash.hpp"

enum class BufferType
{
	ConstantBuffer,
	VertexBuffer,
	StructuredBuffer,
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

	Buffer(const RhiHandle& handle, BufferDescription&& Description)
		: RhiHandle(handle)
		, Description(Move(Description))
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

template<>
struct Hash<Buffer>
{
	uint64 operator()(const Buffer& buffer) const
	{
		const usize handle = buffer.Get();
		return HashFnv1a(&handle, sizeof(handle));
	}
};

inline bool operator==(const Buffer& a, const Buffer& b)
{
	return a.Get() == b.Get();
}

BufferResource AllocateBuffer(ID3D12Device11* device, usize size, bool upload, StringView name);

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
		CHECK(HeapIndex[0] && !HeapIndex[1]);
		return HeapIndex[0];
	}

	uint32 GetHeapIndex(usize index, bool stream) const
	{
		CHECK(index < FramesInFlight);
		const uint32 heapIndex = stream ? HeapIndex[index] : HeapIndex[0];
		CHECK(heapIndex);
		return heapIndex;
	}

	void Write(ID3D12Device11* device, const Buffer& buffer, const void* data, usize frameIndex,
			   Array<UploadPair<Buffer>>* pendingBufferUploads) const;

	BufferResource Resource[FramesInFlight];
	uint32 HeapIndex[FramesInFlight];
};
