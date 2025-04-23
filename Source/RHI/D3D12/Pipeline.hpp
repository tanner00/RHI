#pragma once

#include "Shader.hpp"

#include "RHI/Allocator.hpp"

#include "Luft/String.hpp"

namespace RHI::D3D12
{

inline constexpr usize BindingBucketCount = 4;

class Pipeline : public NoCopy
{
public:
	explicit Pipeline(Device* device)
		: RootParameters(BindingBucketCount, Allocator)
		, RootSignature(nullptr)
		, PipelineState(nullptr)
		, Device(device)
	{
	}
	virtual ~Pipeline();

	virtual void SetConstantBuffer(ID3D12GraphicsCommandList10* commandList, StringView name, const Resource* buffer, usize offset) = 0;
	virtual void SetConstants(ID3D12GraphicsCommandList10* commandList, const void* data) = 0;

	HashTable<String, Dxc::RootParameter> RootParameters;
	ID3D12RootSignature* RootSignature;
	ID3D12PipelineState* PipelineState;
	Device* Device;
};

}
