#pragma once

#include "Pipeline.hpp"

#include "RHI/Forward.hpp"
#include "RHI/GraphicsPipeline.hpp"

namespace RHI::D3D12
{

class GraphicsPipeline final : public Pipeline, GraphicsPipelineDescription
{
public:
	GraphicsPipeline(const GraphicsPipelineDescription& description, D3D12::Device* device);

	virtual void SetConstantBuffer(ID3D12GraphicsCommandList10* commandList, StringView name, const Resource* buffer, usize offset) override;
	virtual void SetConstants(ID3D12GraphicsCommandList10* commandList, const void* data) override;
};

}
