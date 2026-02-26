#include "Shader.hpp"
#include "Base.hpp"
#include "Convert.hpp"
#include "Device.hpp"

#include "D3D12/d3d12shader.h"
#include "dxc/dxcapi.h"

static void ToWideChar(StringView input, wchar_t* output, usize outputLength)
{
	VERIFY(SUCCEEDED(MultiByteToWideChar(CP_UTF8,
										 MB_ERR_INVALID_CHARS,
										 input.GetData(),
										 static_cast<int32>(input.GetLength()),
										 output,
										 static_cast<int32>(outputLength))),
										 "Failed to convert to wide string!");
}

namespace DXC
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
		String resourceName(resourceNameLength, RHI::Allocator);
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
										   .Num32BitValues = static_cast<uint32>(variableDescription.Size / sizeof(uint32)),
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
										   .Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE,
									   },
									   .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
								   });
		}
	}
}

static IDxcResult* CompileShader(StringView filePath, RHI::ShaderStage stage, ArrayView<const RHI::ShaderDefine> defines)
{
	CHECK(Compiler && Utils);

	wchar_t pathBuffer[MAX_PATH] = {};
	ToWideChar(filePath, pathBuffer, ARRAY_COUNT(pathBuffer));

	uint32 codePage = DXC_CP_UTF8;
	IDxcBlobEncoding* sourceBlob = nullptr;
	HRESULT dxcResult = Utils->LoadFile(pathBuffer, &codePage, &sourceBlob);
	VERIFY(SUCCEEDED(dxcResult), "Failed to read shader from file system!");

	const wchar_t* arguments[] =
	{
		L"-HV", L"2021",
		DXC_ARG_WARNINGS_ARE_ERRORS,
		DXC_ARG_ALL_RESOURCES_BOUND,
		L"-enable-16bit-types",
#if DEBUG
		DXC_ARG_OPTIMIZATION_LEVEL0,
		DXC_ARG_SKIP_OPTIMIZATIONS,
#endif
#if RELEASE || PROFILE
		DXC_ARG_OPTIMIZATION_LEVEL3,
#endif
#if DEBUG || PROFILE
		DXC_ARG_DEBUG,
		L"-Qembed_debug",
#endif
#if RELEASE
		L"-Qstrip_debug",
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

	Array<DxcDefine> dxcDefines(defines.GetLength(), RHI::Allocator);
	for (const RHI::ShaderDefine& define : defines)
	{
		wchar_t name[64] = {};
		ToWideChar(define.Name, name, ARRAY_COUNT(name));

		wchar_t value[64] = {};
		ToWideChar(define.Value, value, ARRAY_COUNT(value));

		dxcDefines.Add(DxcDefine
		{
			.Name = name,
			.Value = value,
		});
	}

	IDxcCompilerArgs* compileArguments = nullptr;
	CHECK_RESULT(Utils->BuildArguments(pathBuffer,
									   entryPoint,
									   profile,
									   arguments,
									   ARRAY_COUNT(arguments),
									   dxcDefines.GetData(),
									   static_cast<uint32>(dxcDefines.GetLength()),
									   &compileArguments));

	IDxcIncludeHandler* includeHandler = nullptr;
	CHECK_RESULT(Utils->CreateDefaultIncludeHandler(&includeHandler));

	const DxcBuffer buffer =
	{
		.Ptr = static_cast<uint8*>(sourceBlob->GetBufferPointer()),
		.Size = sourceBlob->GetBufferSize(),
		.Encoding = DXC_CP_UTF8,
	};
	IDxcResult* compileResult = nullptr;
	dxcResult = Compiler->Compile(&buffer,
								  compileArguments->GetArguments(),
								  compileArguments->GetCount(),
								  includeHandler,
								  IID_PPV_ARGS(&compileResult));
	CHECK(SUCCEEDED(dxcResult) && compileResult);

	SAFE_RELEASE(includeHandler);
	SAFE_RELEASE(compileArguments);
	SAFE_RELEASE(sourceBlob);

	const HRESULT statusResult = compileResult->GetStatus(&dxcResult);
	CHECK(SUCCEEDED(statusResult));
#if DEBUG
	if (FAILED(dxcResult))
	{
		IDxcBlobEncoding* errorBlob = nullptr;
		dxcResult = compileResult->GetErrorBuffer(&errorBlob);
		CHECK(SUCCEEDED(dxcResult) && errorBlob);
		char errorMessage[2048];
		Platform::StringPrint("Shader Compiler: %s", errorMessage, sizeof(errorMessage), errorBlob->GetBufferPointer());
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
	IDxcResult* compileResult = DXC::CompileShader(FilePath, Stage, Defines);

	CHECK_RESULT(compileResult->GetResult(&Blob));

	IDxcBlob* reflectionBlob = {};
	CHECK_RESULT(compileResult->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(&reflectionBlob), nullptr));

	const DxcBuffer reflectionBuffer =
	{
		.Ptr = reflectionBlob->GetBufferPointer(),
		.Size = reflectionBlob->GetBufferSize(),
		.Encoding = 0,
	};
	CHECK_RESULT(DXC::Utils->CreateReflection(&reflectionBuffer, IID_PPV_ARGS(&Reflection)));

	SAFE_RELEASE(reflectionBlob);
	SAFE_RELEASE(compileResult);
}

Shader::~Shader()
{
	SAFE_RELEASE(Blob);
	SAFE_RELEASE(Reflection);
}

}
