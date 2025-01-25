#pragma once

#include "Luft/Base.hpp"
#include "Luft/Error.hpp"

#define PAD(size) char TOKEN_PASTE(Pad, __LINE__) [(size)]

class Buffer;
class GpuDevice;
class GraphicsContext;
class GraphicsPipeline;
class Pipeline;
class Sampler;
class Shader;
class Texture;
class ViewHeap;

struct ID3D12CommandAllocator;
struct ID3D12CommandQueue;
struct ID3D12DescriptorHeap;
struct ID3D12Device11;
struct ID3D12GraphicsCommandList10;
struct ID3D12Fence1;
struct ID3D12Heap1;
struct ID3D12PipelineState;
struct ID3D12QueryHeap;
struct ID3D12Resource2;
struct ID3D12RootSignature;
struct ID3D12ShaderReflection;
struct IDXGISwapChain4;
struct IDxcBlob;
struct IUnknown;

template<typename T>
class RhiHandle
{
public:
	RhiHandle()
		: Value(0)
	{
	}

	explicit RhiHandle(usize value)
		: Value(value)
	{
	}

	static T Invalid()
	{
		return T {};
	}

	bool IsValid() const
	{
		return Value != 0;
	}

	usize Get() const
	{
		CHECK(IsValid());
		return Value;
	}

	void Reset()
	{
		Value = 0;
	}

private:
	usize Value;
};

template<typename T>
struct UploadPair
{
	ID3D12Resource2* Source;
	T Destination;
};

enum class ViewType
{
	ConstantBuffer,
	ShaderResource,
	UnorderedAccess,
	Sampler,
	RenderTarget,
	DepthStencil,
	Count,
};

using CpuView = usize;
using GpuView = usize;

using BufferResource = ID3D12Resource2*;
using TextureResource = ID3D12Resource2*;

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

struct Float2
{
	float X;
	float Y;
};

struct Float3
{
	float X;
	float Y;
	float Z;
};

struct Float4
{
	float X;
	float Y;
	float Z;
	float W;
};

inline constexpr uint32 FramesInFlight = 2;
