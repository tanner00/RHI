#include "ComputePipeline.hpp"
#include "Convert.hpp"
#include "Device.hpp"

#include "dxc/dxcapi.h"

namespace RHI::D3D12
{

ComputePipeline::ComputePipeline(const ComputePipelineDescription& description, D3D12::Device* device)
	: Pipeline(device)
	, ComputePipelineDescription(description)
{
	const Shader* backendShader = Stage.Backend;

	HashTable<String, D3D12_ROOT_PARAMETER1> apiRootParameters(BindingBucketCount, Allocator);
	Dxc::ReflectRootParameters(backendShader->Reflection, &RootParameters, &apiRootParameters);

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
	CHECK_RESULT(device->Native->CreateRootSignature(0,
													 serializedRootSignature->GetBufferPointer(),
													 serializedRootSignature->GetBufferSize(),
													 IID_PPV_ARGS(&RootSignature)));
	SET_D3D_NAME(RootSignature, Name);

	const D3D12_COMPUTE_PIPELINE_STATE_DESC computePipelineStateDescription =
	{
		.pRootSignature = RootSignature,
		.CS =
		{
			backendShader->Blob->GetBufferPointer(),
			backendShader->Blob->GetBufferSize(),
		},
		.NodeMask = 0,
		.CachedPSO = {},
		.Flags = D3D12_PIPELINE_STATE_FLAG_NONE,
	};
	CHECK_RESULT(device->Native->CreateComputePipelineState(&computePipelineStateDescription, IID_PPV_ARGS(&PipelineState)));
	SET_D3D_NAME(PipelineState, Name);
}

void ComputePipeline::SetConstantBuffer(ID3D12GraphicsCommandList10* commandList, StringView name, const Resource* buffer, usize offset)
{
	const Dxc::RootParameter& rootParameter = RootParameters[name];

	commandList->SetComputeRootConstantBufferView
	(
		static_cast<uint32>(rootParameter.Index),
		D3D12_GPU_VIRTUAL_ADDRESS { buffer->Native->GetGPUVirtualAddress() + offset }
	);
}

void ComputePipeline::SetConstants(ID3D12GraphicsCommandList10* commandList, const void* data)
{
	static const StringView name = "RootConstants"_view;
	CHECK(RootParameters.Contains(name));

	const Dxc::RootParameter& rootParameter = RootParameters[name];

	commandList->SetComputeRoot32BitConstants
	(
		static_cast<uint32>(rootParameter.Index),
		static_cast<uint32>(rootParameter.Size / sizeof(uint32)),
		data,
		0
	);
}

}
