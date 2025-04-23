#pragma once

#include "Pipeline.hpp"

#include "RHI/ComputePipeline.hpp"

namespace RHI::D3D12
{

class ComputePipeline final : public Pipeline, ComputePipelineDescription
{
public:
	ComputePipeline(const ComputePipelineDescription& description, D3D12::Device* device);

	virtual void SetConstantBuffer(ID3D12GraphicsCommandList10* commandList, StringView name, const Resource* buffer, usize offset) override;
	virtual void SetConstants(ID3D12GraphicsCommandList10* commandList, const void* data) override;
};

}
