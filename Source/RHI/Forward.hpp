#pragma once

#if RHI_D3D12
#include "D3D12/Forward.hpp"
#else
#error "The RHI layer is is currently unimplemented for this backend!"
#endif

namespace RHI
{
class AccelerationStructure;
struct AccelerationStructureGeometry;
struct AccelerationStructureDescription;
struct AccelerationStructureInstance;
struct AccelerationStructureSize;
struct Buffer;
class BufferView;
struct BufferViewDescription;
class ComputePipeline;
struct ComputePipelineDescription;
class Device;
class GraphicsContext;
struct GraphicsContextDescription;
class GraphicsPipeline;
struct GraphicsPipelineDescription;
class Heap;
struct HeapDescription;
class Resource;
struct ResourceDescription;
class Sampler;
struct SamplerDescription;
class Shader;
struct ShaderDescription;
struct SubBuffer;
class TextureView;
struct TextureViewDescription;
}
