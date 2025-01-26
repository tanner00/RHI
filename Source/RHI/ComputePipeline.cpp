#include "ComputePipeline.hpp"
#include "PrivateCommon.hpp"

#include "dxc/dxcapi.h"

D3D12ComputePipeline::D3D12ComputePipeline(ID3D12Device11* device,
										   const ComputePipeline& computePipeline,
										   const HashTable<Shader, D3D12Shader>& apiShaders,
										   StringView name)
	: D3D12Pipeline(PipelineType::Compute)
{
	const D3D12Shader& apiShader = apiShaders.Get(computePipeline.GetStage());

	HashTable<String, D3D12_ROOT_PARAMETER1> apiRootParameters(BindingBucketCount, &GlobalAllocator::Get());
	Dxc::ReflectRootParameters(apiShader.Reflection, &RootParameters, &apiRootParameters);

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

	const D3D12_COMPUTE_PIPELINE_STATE_DESC computePipelineStateDescription =
	{
		.pRootSignature = RootSignature,
		.CS =
		{
			apiShader.Blob->GetBufferPointer(),
			apiShader.Blob->GetBufferSize(),
		},
		.NodeMask = 0,
		.CachedPSO = {},
		.Flags = D3D12_PIPELINE_STATE_FLAG_NONE,
	};
	CHECK_RESULT(device->CreateComputePipelineState(&computePipelineStateDescription, IID_PPV_ARGS(&PipelineState)));
	SET_D3D_NAME(PipelineState, name);
}
