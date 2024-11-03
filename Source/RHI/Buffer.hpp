#pragma once

#include "Common.hpp"

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

class Buffer final : public RHI_HANDLE(Buffer)
{
public:
	RHI_HANDLE_BODY(Buffer);

	usize GetSize() const { return Description.Size; }
	usize GetStride() const { return Description.Stride; }
	usize GetCount() const { return Description.Size / (Description.Stride ? Description.Stride : 1); }

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

class D3D12Buffer
{
public:
	BufferResource GetOnlyBufferResource() const
	{
		CHECK(Resources[0] && !Resources[1]);
		return Resources[0];
	}

	BufferResource GetBufferResource(usize index, bool stream) const
	{
		CHECK(index < FramesInFlight);
		const BufferResource current = stream ? Resources[index] : Resources[0];
		CHECK(current);
		return current;
	}

	BufferResource Resources[FramesInFlight];
};

BufferResource AllocateBuffer(ID3D12Device11* device, usize size, bool upload, StringView name);

D3D12Buffer CreateD3D12Buffer(ID3D12Device11* device, const Buffer& buffer, StringView name);
