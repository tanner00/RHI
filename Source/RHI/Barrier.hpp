#pragma once

#include "Luft/Base.hpp"
#include "Luft/FlagsEnum.hpp"

namespace RHI
{

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
FLAGS_ENUM(BarrierStage);

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
FLAGS_ENUM(BarrierAccess);

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
FLAGS_ENUM(BarrierLayout);

}
