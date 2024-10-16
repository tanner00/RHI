#pragma once

#include "Luft/Base.hpp"
#include "Luft/Error.hpp"
#include "Luft/HashTable.hpp"
#include "Luft/Math.hpp"

#define UINT uint32
#include "D3D12/dxgiformat.h"
#include <dxgicommon.h>
#undef UINT

#define SAFE_RELEASE(p) if ((p)) { (p)->Release(); (p) = nullptr; }

#if DEBUG
#define CHECK_RESULT(expression) do { const HRESULT result = (expression); CHECK(SUCCEEDED(result)); } while (false)
#else
#define CHECK_RESULT(expression) do { (expression); } while (false)
#endif

#define D3DDebugObjectName WKPDID_D3DDebugObjectName

#define PAD(size) char TOKEN_PASTE(Pad, __LINE__) [(size)]

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

enum class DescriptorType
{
	ConstantBuffer,
	ShaderResource,
	UnorderedAccess,
	Sampler,
	RenderTarget,
	DepthStencil,
	Count,
};

struct ID3D12Resource2;
using BufferResource = ID3D12Resource2*;
using TextureResource = ID3D12Resource2*;

using CpuDescriptor = usize;
using GpuDescriptor = usize;

template<typename Tag>
struct RhiHandle
{
	explicit RhiHandle(usize value)
		: Value(value)
	{
	}

	bool IsValid() const
	{
		return Value != 0;
	}

	usize Get() const
	{
		CHECK(Value != 0);
		return Value;
	}

	void Reset()
	{
		Value = 0;
	}

private:
	usize Value;
};

#define DECLARE_RHI_HANDLE(name) struct name##Tag {};

DECLARE_RHI_HANDLE(Buffer);
DECLARE_RHI_HANDLE(Texture);
DECLARE_RHI_HANDLE(Sampler);
DECLARE_RHI_HANDLE(Shader);
DECLARE_RHI_HANDLE(GraphicsPipeline);

#define RHI_HANDLE(name) RhiHandle<name##Tag>

#define RHI_HANDLE_BODY(name)												\
	name##Handle(usize HandleValue, const name##Description& description)	\
		: RhiHandle { HandleValue }											\
		, Description(description)											\
	{}																		\
	name##Handle() : RhiHandle { 0 } {}

enum class BufferType
{
	ConstantBuffer,
	VertexBuffer,
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

class BufferHandle final : public RHI_HANDLE(Buffer)
{
public:
	RHI_HANDLE_BODY(Buffer);

	usize GetSize() const { return Description.Size; }
	usize GetStride() const { return Description.Stride; }
	usize GetCount() const { return Description.Size / (Description.Stride ? Description.Stride : 1); }

	bool IsStatic() const { return Description.Usage == BufferUsage::Static; }
	bool IsDynamic() const { return Description.Usage == BufferUsage::Dynamic; }
	bool IsStream() const { return Description.Usage == BufferUsage::Stream; }

private:
	BufferDescription Description;
};

enum class TextureType
{
	Rectangle,
	Cubemap,
};

enum class TextureFormat
{
	None,

	Rgba8,
	Rgba8Srgb,

	Depth32,
	Depth24Stencil8,
};

struct TextureDescription
{
	uint32 Width;
	uint32 Height;
	TextureType Type;
	TextureFormat Format;
	bool RenderTarget;
};

class TextureHandle final : public RHI_HANDLE(Texture)
{
public:
	RHI_HANDLE_BODY(Texture);

	uint32 GetWidth() const { return Description.Width; }
	uint32 GetHeight() const { return Description.Height; }
	TextureFormat GetFormat() const { return Description.Format; }
	TextureType GetType() const { return Description.Type; }

	bool IsRenderTarget() const { return Description.RenderTarget; }

	usize GetCount() const { return Description.Type == TextureType::Cubemap ? 6 : 1; }
	usize GetComponentCount() const { return 4; }

	uint64 GetRowStride() const
	{
		static constexpr usize TextureDataPitchAlignment = 256;
		return NextMultipleOf(Description.Width * GetComponentCount(), TextureDataPitchAlignment);
	}

private:
	TextureDescription Description;
};

enum class SamplerAddress
{
	Wrap,
	Mirror,
	Clamp,
	Border,
};

enum class SamplerFilter
{
	Point,
	Linear,
	Anisotropic,
};

struct SamplerDescription
{
	SamplerAddress Address;
	SamplerFilter Filter;
	Float4 BorderFilterColor;
};

class SamplerHandle final : public RHI_HANDLE(Sampler)
{
public:
	RHI_HANDLE_BODY(Sampler);

	SamplerAddress GetAddress() const { return Description.Address; }
	SamplerFilter GetFilter() const { return Description.Filter; }

private:
	SamplerDescription Description;
};

enum class ShaderStage
{
	Vertex,
	Pixel,
	Count,
};

template<>
struct Hash<ShaderStage>
{
	uint64 operator()(ShaderStage stage) const
	{
		return HashFnv1a(&stage, sizeof(stage));
	}
};

struct ShaderDescription
{
	ShaderStage Stage;
	StringView FilePath;
};

class ShaderHandle final : public RHI_HANDLE(Shader)
{
public:
	RHI_HANDLE_BODY(Shader);

	ShaderStage GetStage() const { return Description.Stage; }
	StringView GetFilePath() const { return Description.FilePath; }

private:
	ShaderDescription Description;
};

class ShaderStages : public HashTable<ShaderStage, ShaderHandle>
{
public:
	ShaderStages()
		: HashTable(1)
	{
	}

	void AddStage(const ShaderHandle& handle)
	{
		CHECK(!Contains(handle.GetStage()));
		Add(handle.GetStage(), handle);
	}
};

struct GraphicsPipelineDescription
{
	ShaderStages Stages;
	TextureFormat RenderTargetFormat;
	TextureFormat DepthFormat;
};

class GraphicsPipelineHandle final : public RHI_HANDLE(GraphicsPipeline)
{
public:
	RHI_HANDLE_BODY(GraphicsPipeline);

	usize GetStageCount() const { return Description.Stages.GetCount(); }
	bool HasShaderStage(ShaderStage stage) const { return Description.Stages.Contains(stage); }

	ShaderHandle GetShaderStage(ShaderStage stage) const { return Description.Stages[stage]; }

	TextureFormat GetRenderTargetFormat() const { return Description.RenderTargetFormat; }
	TextureFormat GetDepthFormat() const { return Description.DepthFormat; }

private:
	GraphicsPipelineDescription Description;
};

inline constexpr uint32 FramesInFlight = 2;
inline constexpr DXGI_SAMPLE_DESC DefaultSampleDescriptor = { 1, 0 };

inline DXGI_FORMAT ToD3D12(TextureFormat format)
{
	switch (format)
	{
	case TextureFormat::None:
		return DXGI_FORMAT_UNKNOWN;
	case TextureFormat::Rgba8:
		return DXGI_FORMAT_R8G8B8A8_UNORM;
	case TextureFormat::Rgba8Srgb:
		return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	case TextureFormat::Depth24Stencil8:
		return DXGI_FORMAT_D24_UNORM_S8_UINT;
	case TextureFormat::Depth32:
		return DXGI_FORMAT_D32_FLOAT;
	}
	CHECK(false);
	return DXGI_FORMAT_UNKNOWN;
}

inline bool IsDepthFormat(TextureFormat format)
{
	return format == TextureFormat::Depth32 || format == TextureFormat::Depth24Stencil8;
}

inline bool IsStencilFormat(TextureFormat format)
{
	return format == TextureFormat::Depth24Stencil8;
}

class GpuDevice;
class GraphicsContext;

struct ID3D12CommandAllocator;
struct ID3D12CommandQueue;
struct ID3D12DescriptorHeap;
struct ID3D12Device10;
struct ID3D12GraphicsCommandList10;
struct ID3D12Fence1;
struct ID3D12Heap1;
struct ID3D12PipelineState;
struct ID3D12RootSignature;
struct ID3D12ShaderReflection;
struct IDXGISwapChain4;
struct IDxcBlob;
struct IUnknown;
