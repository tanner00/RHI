#pragma once

#include "Allocator.hpp"
#include "Forward.hpp"
#include "Resource.hpp"
#include "Shader.hpp"

#include "Luft/HashTable.hpp"

namespace RHI
{

class ShaderStages : public HashTable<ShaderStage, Shader>
{
public:
	ShaderStages()
		: HashTable(1, RHI::Allocator)
	{
	}

	void AddStage(const Shader& shader)
	{
		CHECK(!Contains(shader.Stage));
		Add(shader.Stage, shader);
	}
};

struct GraphicsPipelineDescription
{
	ShaderStages Stages;

	ArrayView<const ResourceFormat> RenderTargetFormats;
	ResourceFormat DepthStencilFormat;

	bool AlphaBlend;
	bool ReverseDepth;

	StringView Name;
};

class GraphicsPipeline final : public GraphicsPipelineDescription
{
public:
	GraphicsPipeline()
		: GraphicsPipelineDescription()
		, Backend(nullptr)
	{
	}

	GraphicsPipeline(const GraphicsPipelineDescription& description, RHI_BACKEND(GraphicsPipeline)* backend)
		: GraphicsPipelineDescription(description)
		, Backend(backend)
	{
	}

	static GraphicsPipeline Invalid() { return {}; }
	bool IsValid() const { return Backend != nullptr; }

	RHI_BACKEND(GraphicsPipeline)* Backend;
};

}
