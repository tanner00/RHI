#include "GraphicsPipeline.hpp"
#include "PrivateCommon.hpp"

#include "dxc/dxcapi.h"

D3D12GraphicsPipeline::D3D12GraphicsPipeline(ID3D12Device11* device,
											 const GraphicsPipeline& graphicsPipeline,
											 const HashTable<Shader, D3D12Shader>& apiShaders,
											 StringView name)
	: D3D12Pipeline(PipelineType::Graphics)
{
	CHECK(graphicsPipeline.HasShaderStage(ShaderStage::Vertex));
	const bool usesPixelShader = graphicsPipeline.HasShaderStage(ShaderStage::Pixel);
	if (usesPixelShader)
	{
		CHECK(graphicsPipeline.GetStageCount() == 2);
	}
	else
	{
		CHECK(graphicsPipeline.GetStageCount() == 1);
	}

	const D3D12Shader* vertex = &apiShaders[graphicsPipeline.GetShaderStage(ShaderStage::Vertex)];
	const D3D12Shader* pixel = usesPixelShader ? &apiShaders[graphicsPipeline.GetShaderStage(ShaderStage::Pixel)] : nullptr;

	Array<D3D12_INPUT_ELEMENT_DESC> inputElements(&GlobalAllocator::Get());
	Dxc::ReflectInputElements(vertex->Reflection, inputElements);

	HashTable<String, D3D12_ROOT_PARAMETER1> apiRootParameters(BindingBucketCount, &GlobalAllocator::Get());
	Dxc::ReflectRootParameters(vertex->Reflection, &RootParameters, &apiRootParameters);
	if (usesPixelShader)
	{
		Dxc::ReflectRootParameters(pixel->Reflection, &RootParameters, &apiRootParameters);
	}

	Array<D3D12_ROOT_PARAMETER1> rootParametersList(apiRootParameters.GetCount(), &GlobalAllocator::Get());
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
	CHECK_RESULT(device->CreateRootSignature(0, serializedRootSignature->GetBufferPointer(), serializedRootSignature->GetBufferSize(),
											 IID_PPV_ARGS(&RootSignature)));
	SET_D3D_NAME(RootSignature, name);

	const D3D12_RENDER_TARGET_BLEND_DESC defaultBlendDescription =
	{
		.BlendEnable = graphicsPipeline.IsAlphaBlended(),
		.LogicOpEnable = false,
		.SrcBlend = graphicsPipeline.IsAlphaBlended() ? D3D12_BLEND_SRC_ALPHA : D3D12_BLEND_ONE,
		.DestBlend = graphicsPipeline.IsAlphaBlended() ? D3D12_BLEND_INV_SRC_ALPHA : D3D12_BLEND_ZERO,
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
			.pShaderBytecode = vertex->Blob->GetBufferPointer(),
			.BytecodeLength = vertex->Blob->GetBufferSize(),
		},
		.PS =
		{
			.pShaderBytecode = usesPixelShader ? pixel->Blob->GetBufferPointer() : nullptr,
			.BytecodeLength = usesPixelShader ? pixel->Blob->GetBufferSize() : 0,
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
			.DepthEnable = IsDepthFormat(graphicsPipeline.GetDepthFormat()),
			.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL,
			.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL,
			.StencilEnable = IsStencilFormat(graphicsPipeline.GetDepthFormat()),
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
		.NumRenderTargets = usesPixelShader ? 1U : 0U,
		.RTVFormats =
		{
			ToD3D12(graphicsPipeline.GetRenderTargetFormat()),
		},
		.DSVFormat = ToD3D12(graphicsPipeline.GetDepthFormat()),
		.SampleDesc = DefaultSampleDescription,
		.NodeMask = 0,
		.CachedPSO = {},
		.Flags = D3D12_PIPELINE_STATE_FLAG_NONE,
	};
	CHECK_RESULT(device->CreateGraphicsPipelineState(&graphicsPipelineStateDescription, IID_PPV_ARGS(&PipelineState)));
	SET_D3D_NAME(PipelineState, name);
}
