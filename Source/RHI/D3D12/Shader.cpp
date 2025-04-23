#include "Shader.hpp"
#include "Base.hpp"
#include "Convert.hpp"
#include "Device.hpp"

#include "D3D12/d3d12shader.h"
#include "dxc/dxcapi.h"

namespace Dxc
{

static IDxcCompiler3* Compiler = nullptr;
static IDxcUtils* Utils = nullptr;

void Init()
{
	constexpr IMalloc* dxcAllocator = nullptr;
	HRESULT result = DxcCreateInstance2(dxcAllocator, CLSID_DxcCompiler, IID_PPV_ARGS(&Compiler));
	CHECK(SUCCEEDED(result));
	result = DxcCreateInstance2(dxcAllocator, CLSID_DxcUtils, IID_PPV_ARGS(&Utils));
	CHECK(SUCCEEDED(result));
}

void Shutdown()
{
	SAFE_RELEASE(Utils);
	SAFE_RELEASE(Compiler);
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

		inputElements.Add(D3D12_INPUT_ELEMENT_DESC
		{
			.SemanticName = inputParameterDescription.SemanticName,
			.SemanticIndex = inputParameterDescription.SemanticIndex,
			.Format = RHI::D3D12::MaskToFormat(inputParameterDescription.Mask),
			.InputSlot = inputParameterDescription.Register,
			.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT,
			.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			.InstanceDataStepRate = 0,
		});
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
		String resourceName = String { resourceNameLength, RHI::Allocator };
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
		const bool rootConstant = Platform::StringCompare(resourceDescription.Name,
														  Platform::StringLength(resourceDescription.Name),
														  rootConstantsName,
														  sizeof(rootConstantsName));

		if (rootConstant)
		{
			apiRootParameters->Add(Move(resourceName),
								   D3D12_ROOT_PARAMETER1
								   {
									   .ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
									   .Constants =
									   {
										   .ShaderRegister = resourceDescription.BindPoint,
										   .RegisterSpace = resourceDescription.Space,
										   .Num32BitValues = static_cast<uint32>(variableDescription
											   .Size / sizeof(uint32)),
									   },
									   .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
								   });
		}
		else
		{
			apiRootParameters->Add(Move(resourceName),
								   D3D12_ROOT_PARAMETER1
								   {
									   .ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV,
									   .Descriptor =
									   {
										   .ShaderRegister = resourceDescription.BindPoint,
										   .RegisterSpace = resourceDescription.Space,
										   .Flags =
										   D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE,
									   },
									   .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
								   });
		}
	}
}

static IDxcResult* CompileShader(RHI::ShaderStage stage, StringView filePath)
{
	CHECK(Compiler && Utils);

	wchar_t pathBuffer[MAX_PATH] = {};
	VERIFY(filePath.GetLength() < sizeof(pathBuffer), "File path length limit exceeded!");

	const int32 result = MultiByteToWideChar(CP_UTF8,
											 MB_ERR_INVALID_CHARS,
											 filePath.GetData(),
											 static_cast<int32>(filePath.GetLength()),
											 pathBuffer,
											 ARRAY_COUNT(pathBuffer));
	CHECK(SUCCEEDED(result));

	uint32 codePage = DXC_CP_UTF8;
	IDxcBlobEncoding* sourceBlob = nullptr;
	HRESULT dxcResult = Utils->LoadFile(pathBuffer, &codePage, &sourceBlob);
	VERIFY(SUCCEEDED(dxcResult), "Failed to read shader from file system!");

	const wchar_t* arguments[] =
	{
		DXC_ARG_WARNINGS_ARE_ERRORS,
		L"-HV", L"2021",
		L"-all_resources_bound",
#if DEBUG
		DXC_ARG_OPTIMIZATION_LEVEL0,
		DXC_ARG_SKIP_OPTIMIZATIONS,
#endif
#if RELEASE
		L"-Qstrip_debug",
#endif
#if DEBUG || PROFILE
		DXC_ARG_DEBUG,
		L"-Qembed_debug",
#endif
#if RELEASE || PROFILE
		DXC_ARG_OPTIMIZATION_LEVEL3,
#endif
	};

	LPCWSTR entryPoint = nullptr;
	LPCWSTR profile = nullptr;
	switch (stage)
	{
	case RHI::ShaderStage::Vertex:
		entryPoint = L"VertexStart";
		profile = L"vs_6_6";
		break;
	case RHI::ShaderStage::Pixel:
		entryPoint = L"PixelStart";
		profile = L"ps_6_6";
		break;
	case RHI::ShaderStage::Compute:
		entryPoint = L"ComputeStart";
		profile = L"cs_6_6";
		break;
	default:
		CHECK(false);
	}
	constexpr const DxcDefine* defines = nullptr;

	IDxcCompilerArgs* compileArguments = nullptr;
	CHECK_RESULT(Utils->BuildArguments(pathBuffer, entryPoint, profile, arguments, ARRAY_COUNT(arguments), defines, 0, &compileArguments));

	IDxcIncludeHandler* includeHandler = nullptr;
	CHECK_RESULT(Utils->CreateDefaultIncludeHandler(&includeHandler));

	const DxcBuffer buffer =
	{
		.Ptr = static_cast<uint8*>(sourceBlob->GetBufferPointer()),
		.Size = sourceBlob->GetBufferSize(),
		.Encoding = DXC_CP_UTF8,
	};
	IDxcResult* compileResult = nullptr;
	dxcResult = Compiler->Compile(&buffer, compileArguments->GetArguments(), compileArguments->GetCount(), includeHandler, IID_PPV_ARGS(&compileResult));
	CHECK(SUCCEEDED(dxcResult) && compileResult);

	SAFE_RELEASE(includeHandler);
	SAFE_RELEASE(compileArguments);
	SAFE_RELEASE(sourceBlob);

	const HRESULT statusResult = compileResult->GetStatus(&dxcResult);
	CHECK(SUCCEEDED(statusResult));
#if DEBUG
	if (FAILED(dxcResult))
	{
		IDxcBlobEncoding* dxcErrorBlob = nullptr;
		dxcResult = compileResult->GetErrorBuffer(&dxcErrorBlob);
		CHECK(SUCCEEDED(dxcResult) && dxcErrorBlob);
		char errorMessage[2048];
		Platform::StringPrint("Shader Compiler: %s", errorMessage, sizeof(errorMessage), dxcErrorBlob->GetBufferPointer());
		Platform::FatalError(errorMessage);
	}
#endif
	return compileResult;
}

}

namespace RHI::D3D12
{

Shader::Shader(const ShaderDescription& description, D3D12::Device* device)
	: ShaderDescription(description)
	, Device(device)
{
	IDxcResult* compileResult = Dxc::CompileShader(Stage, FilePath);

	CHECK_RESULT(compileResult->GetResult(&Blob));

	IDxcBlob* reflectionBlob = {};
	CHECK_RESULT(compileResult->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(&reflectionBlob), nullptr));

	const DxcBuffer reflectionBuffer =
	{
		.Ptr = reflectionBlob->GetBufferPointer(),
		.Size = reflectionBlob->GetBufferSize(),
		.Encoding = 0,
	};
	CHECK_RESULT(Dxc::Utils->CreateReflection(&reflectionBuffer, IID_PPV_ARGS(&Reflection)));

	SAFE_RELEASE(reflectionBlob);
	SAFE_RELEASE(compileResult);
}

Shader::~Shader()
{
	Device->AddPendingDelete(Blob);
	Device->AddPendingDelete(Reflection);
}

}
