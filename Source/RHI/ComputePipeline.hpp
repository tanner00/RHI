#pragma once

#include "Pipeline.hpp"

struct ComputePipelineDescription
{
	Shader Stage;
};

class ComputePipeline final : public Pipeline
{
public:
	ComputePipeline()
		: Pipeline(RhiHandle(0))
		, Description()
	{
	}

	ComputePipeline(const RhiHandle& handle, ComputePipelineDescription&& Description)
		: Pipeline(handle)
		, Description(Move(Description))
	{
	}

	Shader GetStage() const { return Description.Stage; }

private:
	ComputePipelineDescription Description;
};

class D3D12ComputePipeline : public D3D12Pipeline
{
public:
	D3D12ComputePipeline(ID3D12Device11* device,
						 const ComputePipeline& computePipeline,
						 const HashTable<Shader, D3D12Shader>& apiShaders,
						 StringView name);
};
