#include "GraphicsPipeline.hpp"
#include "PrivateCommon.hpp"

#include "D3D12/d3d12.h"
#include "dxc/d3d12shader.h"
#include "dxc/dxcapi.h"

namespace Dxc
{
static void ReflectInputElements(ID3D12ShaderReflection* shaderReflection, Array<D3D12_INPUT_ELEMENT_DESC>& inputElements);
static void ReflectRootParameters(ID3D12ShaderReflection* shaderReflection,
								  HashTable<String, RootParameter>* rootParameters,
								  HashTable<String, D3D12_ROOT_PARAMETER1>* apiRootParameters);
}

static constexpr usize BindingBucketCount = 4;

D3D12GraphicsPipeline::D3D12GraphicsPipeline(ID3D12Device11* device, const GraphicsPipeline& graphicsPipeline,
											 const HashTable<Shader, D3D12Shader>& apiShaders, StringView name)
	: RootParameters(BindingBucketCount, &GlobalAllocator::Get())
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
	SetD3DName(RootSignature, name);

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
	SetD3DName(PipelineState, name);
}

namespace Dxc
{

static DXGI_FORMAT MaskToFormat(uint8 mask)
{
	switch (mask)
	{
	case 0b1:
		return DXGI_FORMAT_R32_FLOAT;
	case 0b11:
		return DXGI_FORMAT_R32G32_FLOAT;
	case 0b111:
		return DXGI_FORMAT_R32G32B32_FLOAT;
	case 0b1111:
		return DXGI_FORMAT_R32G32B32A32_FLOAT;
	default:
		break;
	}
	CHECK(false);
	return DXGI_FORMAT_UNKNOWN;
}

static void ReflectInputElements(ID3D12ShaderReflection* shaderReflection, Array<D3D12_INPUT_ELEMENT_DESC>& inputElements)
{
	D3D12_SHADER_DESC shaderDescription = {};
	CHECK_RESULT(shaderReflection->GetDesc(&shaderDescription));

	for (uint32 i = 0; i < shaderDescription.InputParameters; ++i)
	{
		D3D12_SIGNATURE_PARAMETER_DESC inputParameterDescription = {};
		CHECK_RESULT(shaderReflection->GetInputParameterDesc(i, &inputParameterDescription));

		const bool systemValue = inputParameterDescription.SystemValueType != D3D_NAME_UNDEFINED;
		if (systemValue)
		{
			continue;
		}

		inputElements.Add
		(
			D3D12_INPUT_ELEMENT_DESC
			{
				.SemanticName = inputParameterDescription.SemanticName,
				.SemanticIndex = inputParameterDescription.SemanticIndex,
				.Format = MaskToFormat(inputParameterDescription.Mask),
				.InputSlot = inputParameterDescription.Register,
				.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT,
				.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
				.InstanceDataStepRate = 0,
			}
		);
	}
}

static void ReflectRootParameters(ID3D12ShaderReflection* shaderReflection,
								  HashTable<String, RootParameter>* rootParameters,
								  HashTable<String, D3D12_ROOT_PARAMETER1>* apiRootParameters)
{
	D3D12_SHADER_DESC shaderDescription = {};
	CHECK_RESULT(shaderReflection->GetDesc(&shaderDescription));

	for (uint32 i = 0; i < shaderDescription.BoundResources; ++i)
	{
		D3D12_SHADER_INPUT_BIND_DESC resourceDescription = {};
		CHECK_RESULT(shaderReflection->GetResourceBindingDesc(i, &resourceDescription));

		CHECK(resourceDescription.Type == D3D_SIT_CBUFFER);

		const usize resourceNameLength = Platform::StringLength(resourceDescription.Name);
		String resourceName = String { resourceNameLength, &GlobalAllocator::Get() };
		for (usize j = 0; j < resourceNameLength; ++j)
		{
			resourceName.Append(resourceDescription.Name[j]);
		}

		D3D12_SHADER_VARIABLE_DESC variableDescription = {};
		ID3D12ShaderReflectionVariable* variable = shaderReflection->GetVariableByName(resourceDescription.Name);
		CHECK_RESULT(variable->GetDesc(&variableDescription));

		rootParameters->Add(resourceName, RootParameter
		{
			.Index = i,
			.Size = variableDescription.Size,
		});

		static constexpr char rootConstantsName[] = "RootConstants";
		const bool rootConstant = Platform::StringCompare(resourceDescription.Name, Platform::StringLength(resourceDescription.Name),
														  rootConstantsName, sizeof(rootConstantsName));

		if (rootConstant)
		{
			apiRootParameters->Add
			(
				Move(resourceName),
				D3D12_ROOT_PARAMETER1
				{
					.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
					.Constants =
					{
						.ShaderRegister = resourceDescription.BindPoint,
						.RegisterSpace = resourceDescription.Space,
						.Num32BitValues = static_cast<uint32>(variableDescription.Size / sizeof(uint32)),
					},
					.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
				}
			);
		}
		else
		{
			apiRootParameters->Add
			(
				Move(resourceName),
				D3D12_ROOT_PARAMETER1
				{
					.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV,
					.Descriptor =
					{
						.ShaderRegister = resourceDescription.BindPoint,
						.RegisterSpace = resourceDescription.Space,
						.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE,
					},
					.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
				}
			);
		}
	}
}

}
