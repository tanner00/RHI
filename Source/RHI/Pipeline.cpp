#include "Pipeline.hpp"
#include "PrivateCommon.hpp"

#include "dxc/d3d12shader.h"

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

void ReflectInputElements(ID3D12ShaderReflection* shaderReflection, Array<D3D12_INPUT_ELEMENT_DESC>& inputElements)
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

void ReflectRootParameters(ID3D12ShaderReflection* shaderReflection,
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
