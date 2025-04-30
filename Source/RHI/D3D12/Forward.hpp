#pragma once

#define RHI_BACKEND(name) D3D12::##name

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

namespace RHI::D3D12
{
class AccelerationStructure;
class BufferView;
class ComputePipeline;
class Device;
class GraphicsContext;
class GraphicsPipeline;
class Pipeline;
class Resource;
class Sampler;
class Shader;
class TextureView;
}
