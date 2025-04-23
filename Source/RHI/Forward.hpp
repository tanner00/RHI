#pragma once

#if RHI_D3D12
#include "D3D12/Forward.hpp"
#else
#error "The RHI layer is is currently unimplemented for this backend!"
#endif

namespace RHI
{
class BufferView;
class ComputePipeline;
class Device;
class GraphicsContext;
class GraphicsPipeline;
class Resource;
class Sampler;
class Shader;
class TextureView;
}
