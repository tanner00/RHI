#pragma once

#include "Common.hpp"
#include "Pipeline.hpp"
#include "Texture.hpp"

#include "Luft/HashTable.hpp"

class ShaderStages : public HashTable<ShaderStage, Shader>
{
public:
	ShaderStages()
		: HashTable(1, &GlobalAllocator::Get())
	{
	}

	void AddStage(const Shader& shader)
	{
		CHECK(!Contains(shader.GetStage()));
		Add(shader.GetStage(), shader);
	}
};

struct GraphicsPipelineDescription
{
	ShaderStages Stages;
	TextureFormat RenderTargetFormat;
	TextureFormat DepthFormat;
	bool AlphaBlend;
};

class GraphicsPipeline final : public Pipeline
{
public:
	GraphicsPipeline()
		: Pipeline(RhiHandle(0))
		, Description()
	{
	}

	GraphicsPipeline(const RhiHandle& handle, GraphicsPipelineDescription&& Description)
		: Pipeline(handle)
		, Description(Move(Description))
	{
	}

	usize GetStageCount() const { return Description.Stages.GetCount(); }
	bool HasShaderStage(ShaderStage stage) const { return Description.Stages.Contains(stage); }
	Shader GetShaderStage(ShaderStage stage) const { return Description.Stages[stage]; }

	TextureFormat GetRenderTargetFormat() const { return Description.RenderTargetFormat; }
	TextureFormat GetDepthFormat() const { return Description.DepthFormat; }

	bool IsAlphaBlended() const { return Description.AlphaBlend; }

private:
	GraphicsPipelineDescription Description;
};

class D3D12GraphicsPipeline final : public D3D12Pipeline
{
public:
	D3D12GraphicsPipeline(ID3D12Device11* device,
						  const GraphicsPipeline& graphicsPipeline,
						  const HashTable<Shader, D3D12Shader>& apiShaders,
						  StringView name);
};
