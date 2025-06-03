#include "GraphicsPipeline.hpp"
#include "Base.hpp"
#include "Convert.hpp"
#include "Device.hpp"

#include "dxc/dxcapi.h"

namespace RHI::D3D12
{

GraphicsPipeline::GraphicsPipeline(const GraphicsPipelineDescription& description, D3D12::Device* device)
	: Pipeline(device)
	, GraphicsPipelineDescription(description)
{
	CHECK(RenderTargetFormats.GetLength() <= MaxRenderTargetCount);

	CHECK(Stages.Contains(ShaderStage::Vertex));
	const bool usesPixelShader = Stages.Contains(ShaderStage::Pixel);
	CHECK(usesPixelShader ? (Stages.GetCount() == 2) : (Stages.GetCount() == 1));

	const Shader* backendVertexShader = Stages[ShaderStage::Vertex].Backend;
	const Shader* backendPixelShader = usesPixelShader ? Stages[ShaderStage::Pixel].Backend : nullptr;

	Array<D3D12_INPUT_ELEMENT_DESC> inputElements(Allocator);
	Dxc::ReflectInputElements(backendVertexShader->Reflection, inputElements);

	HashTable<String, D3D12_ROOT_PARAMETER1> apiRootParameters(BindingBucketCount, Allocator);
	Dxc::ReflectRootParameters(backendVertexShader->Reflection, &RootParameters, &apiRootParameters);
	if (usesPixelShader)
	{
		Dxc::ReflectRootParameters(backendPixelShader->Reflection, &RootParameters, &apiRootParameters);
	}

	Array<D3D12_ROOT_PARAMETER1> rootParametersList(apiRootParameters.GetCount(), Allocator);
	for (auto& [_, rootParameter] : apiRootParameters)
	{
		rootParametersList.Add(rootParameter);
	}

	const D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription =
	{
		.Version = D3D_ROOT_SIGNATURE_VERSION_1_1,
		.Desc_1_1 =
		{
			.NumParameters = static_cast<uint32>(rootParametersList.GetLength()),
			.pParameters = rootParametersList.GetData(),
			.NumStaticSamplers = 0,
			.pStaticSamplers = nullptr,
			.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
					 D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED |
					 D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED,
		},
	};
	ID3DBlob* serializedRootSignature = nullptr;
	ID3DBlob* errorBlob = nullptr;
	const HRESULT rootSignatureResult = D3D12SerializeVersionedRootSignature(&rootSignatureDescription, &serializedRootSignature, &errorBlob);
#if DEBUG
	if (FAILED(rootSignatureResult) && errorBlob)
	{
		char errorMessage[512] = {};
		Platform::StringPrint("Root Signature Error: %s\n", errorMessage, sizeof(errorMessage), errorBlob->GetBufferPointer());
		Platform::FatalError(errorMessage);
	}
#else
	(void)rootSignatureResult;
#endif
	CHECK(serializedRootSignature);
	CHECK_RESULT(device->Native->CreateRootSignature(0, serializedRootSignature->GetBufferPointer(), serializedRootSignature->GetBufferSize(),
													 IID_PPV_ARGS(&RootSignature)));
	SET_D3D_NAME(RootSignature, Name);

	const D3D12_RENDER_TARGET_BLEND_DESC defaultBlendDescription =
	{
		.BlendEnable = AlphaBlend,
		.LogicOpEnable = false,
		.SrcBlend = AlphaBlend ? D3D12_BLEND_SRC_ALPHA : D3D12_BLEND_ONE,
		.DestBlend = AlphaBlend ? D3D12_BLEND_INV_SRC_ALPHA : D3D12_BLEND_ZERO,
		.BlendOp = D3D12_BLEND_OP_ADD,
		.SrcBlendAlpha = D3D12_BLEND_ONE,
		.DestBlendAlpha = D3D12_BLEND_ZERO,
		.BlendOpAlpha = D3D12_BLEND_OP_ADD,
		.LogicOp = D3D12_LOGIC_OP_NOOP,
		.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL,
	};
	const D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDescription =
	{
		.pRootSignature = RootSignature,
		.VS =
		{
			.pShaderBytecode = backendVertexShader->Blob->GetBufferPointer(),
			.BytecodeLength = backendVertexShader->Blob->GetBufferSize(),
		},
		.PS =
		{
			.pShaderBytecode = usesPixelShader ? backendPixelShader->Blob->GetBufferPointer() : nullptr,
			.BytecodeLength = usesPixelShader ? backendPixelShader->Blob->GetBufferSize() : 0,
		},
		.StreamOutput = {},
		.BlendState =
		{
			.AlphaToCoverageEnable = false,
			.IndependentBlendEnable = false,
			.RenderTarget =
			{
				defaultBlendDescription,
			},
		},
		.SampleMask = D3D12_DEFAULT_SAMPLE_MASK,
		.RasterizerState =
		{
			.FillMode = D3D12_FILL_MODE_SOLID,
			.CullMode = D3D12_CULL_MODE_BACK,
			.FrontCounterClockwise = true,
			.DepthBias = D3D12_DEFAULT_DEPTH_BIAS,
			.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP,
			.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
			.DepthClipEnable = true,
			.MultisampleEnable = false,
			.AntialiasedLineEnable = false,
			.ForcedSampleCount = 0,
			.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF,
		},
		.DepthStencilState =
		{
			.DepthEnable = IsDepthFormat(DepthStencilFormat),
			.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL,
			.DepthFunc = description.ReverseDepth ? D3D12_COMPARISON_FUNC_GREATER_EQUAL : D3D12_COMPARISON_FUNC_LESS_EQUAL,
			.StencilEnable = IsStencilFormat(DepthStencilFormat),
			.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK,
			.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK,
			.FrontFace =
			{
				.StencilFailOp = D3D12_STENCIL_OP_KEEP,
				.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP,
				.StencilPassOp = D3D12_STENCIL_OP_KEEP,
				.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS
			},
			.BackFace =
			{
				.StencilFailOp = D3D12_STENCIL_OP_KEEP,
				.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP,
				.StencilPassOp = D3D12_STENCIL_OP_KEEP,
				.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS
			},
		},
		.InputLayout =
		{
			.pInputElementDescs = inputElements.GetData(),
			.NumElements = static_cast<uint32>(inputElements.GetLength()),
		},
		.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED,
		.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		.NumRenderTargets = usesPixelShader ? static_cast<uint32>(RenderTargetFormats.GetLength()) : 0,
		.RTVFormats =
		{
			RenderTargetFormats.GetLength() > 0 ? To(RenderTargetFormats[0]) : DXGI_FORMAT_UNKNOWN,
			RenderTargetFormats.GetLength() > 1 ? To(RenderTargetFormats[1]) : DXGI_FORMAT_UNKNOWN,
			RenderTargetFormats.GetLength() > 2 ? To(RenderTargetFormats[2]) : DXGI_FORMAT_UNKNOWN,
			RenderTargetFormats.GetLength() > 3 ? To(RenderTargetFormats[3]) : DXGI_FORMAT_UNKNOWN,
			RenderTargetFormats.GetLength() > 4 ? To(RenderTargetFormats[4]) : DXGI_FORMAT_UNKNOWN,
			RenderTargetFormats.GetLength() > 5 ? To(RenderTargetFormats[5]) : DXGI_FORMAT_UNKNOWN,
			RenderTargetFormats.GetLength() > 6 ? To(RenderTargetFormats[6]) : DXGI_FORMAT_UNKNOWN,
			RenderTargetFormats.GetLength() > 7 ? To(RenderTargetFormats[7]) : DXGI_FORMAT_UNKNOWN,
		},
		.DSVFormat = To(DepthStencilFormat),
		.SampleDesc = DefaultSampleDescription,
		.NodeMask = 0,
		.CachedPSO = {},
		.Flags = D3D12_PIPELINE_STATE_FLAG_NONE,
	};
	CHECK_RESULT(device->Native->CreateGraphicsPipelineState(&graphicsPipelineStateDescription, IID_PPV_ARGS(&PipelineState)));
	SET_D3D_NAME(PipelineState, Name);
}

void GraphicsPipeline::SetConstantBuffer(ID3D12GraphicsCommandList10* commandList, StringView name, const Resource* buffer, usize offset)
{
	CHECK(RootParameters.Contains(name));
	const usize rootParameterIndex = RootParameters[name].Index;

	commandList->SetGraphicsRootConstantBufferView
	(
		static_cast<uint32>(rootParameterIndex),
		D3D12_GPU_VIRTUAL_ADDRESS { buffer->Native->GetGPUVirtualAddress() + offset }
	);
}

void GraphicsPipeline::SetConstants(ID3D12GraphicsCommandList10* commandList, const void* data)
{
	static const StringView name = "RootConstants"_view;
	CHECK(RootParameters.Contains(name));

	const Dxc::RootParameter& rootParameter = RootParameters[name];

	commandList->SetGraphicsRoot32BitConstants
	(
		static_cast<uint32>(rootParameter.Index),
		static_cast<uint32>(rootParameter.Size / sizeof(uint32)),
		data,
		0
	);
}

}
