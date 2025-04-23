#pragma once

#include "Shader.hpp"

namespace RHI
{

struct ComputePipelineDescription
{
	Shader Stage;

	StringView Name;
};

class ComputePipeline final : public ComputePipelineDescription
{
public:
	ComputePipeline()
		: ComputePipelineDescription()
		, Backend(nullptr)
	{
	}

	ComputePipeline(const ComputePipelineDescription& description, RHI_BACKEND(ComputePipeline)* backend)
		: ComputePipelineDescription(description)
		, Backend(backend)
	{
	}

	static ComputePipeline Invalid() { return {}; }
	bool IsValid() const { return Backend != nullptr; }

	RHI_BACKEND(ComputePipeline)* Backend;
};

}
