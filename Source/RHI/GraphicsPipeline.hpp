#pragma once

#include "Common.hpp"
#include "Shader.hpp"
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

class GraphicsPipeline final : public RHI_HANDLE(GraphicsPipeline)
{
public:
	RHI_HANDLE_BODY(GraphicsPipeline);

	usize GetStageCount() const { return Description.Stages.GetCount(); }
	bool HasShaderStage(ShaderStage stage) const { return Description.Stages.Contains(stage); }
	Shader GetShaderStage(ShaderStage stage) const { return Description.Stages[stage]; }

	TextureFormat GetRenderTargetFormat() const { return Description.RenderTargetFormat; }
	TextureFormat GetDepthFormat() const { return Description.DepthFormat; }

	bool IsAlphaBlended() const { return Description.AlphaBlend; }

private:
	GraphicsPipelineDescription Description;
};

template<>
struct Hash<GraphicsPipeline>
{
	uint64 operator()(const GraphicsPipeline& graphicsPipeline) const
	{
		const usize handle = graphicsPipeline.Get();
		return HashFnv1a(&handle, sizeof(handle));
	}
};

inline bool operator==(const GraphicsPipeline& a, const GraphicsPipeline& b)
{
	return a.Get() == b.Get();
}

struct RootParameter
{
	usize Index;
	usize Size;
};

class D3D12GraphicsPipeline
{
public:
	D3D12GraphicsPipeline(ID3D12Device11* device, const GraphicsPipeline& graphicsPipeline,
						  const HashTable<Shader, D3D12Shader>& apiShaders, StringView name);

	HashTable<String, RootParameter> RootParameters;
	ID3D12RootSignature* RootSignature;
	ID3D12PipelineState* PipelineState;
};
