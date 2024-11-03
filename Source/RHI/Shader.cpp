#include "Shader.hpp"

#include "D3D12/d3d12.h"
#include "dxc/d3d12shader.h"
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

static IDxcResult* CompileShader(ShaderStage stage, StringView filePath)
{
	CHECK(Compiler && Utils);

	wchar_t pathBuffer[MAX_PATH];
	VERIFY(filePath.GetLength() < sizeof(pathBuffer), "File path length limit exceeded!");
	const errno_t error = mbstowcs_s(nullptr, pathBuffer, reinterpret_cast<const char*>(filePath.GetData()), filePath.GetLength());
	CHECK(error == 0);

	uint32 codePage = DXC_CP_UTF8;
	IDxcBlobEncoding* sourceBlob = nullptr;
	HRESULT dxcResult = Utils->LoadFile(pathBuffer, &codePage, &sourceBlob);
	VERIFY(SUCCEEDED(dxcResult), "Failed to read shader from filesystem!");

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
	case ShaderStage::Vertex:
		entryPoint = L"VertexMain";
		profile = L"vs_6_6";
		break;
	case ShaderStage::Pixel:
		entryPoint = L"PixelMain";
		profile = L"ps_6_6";
		break;
	default:
		CHECK(false);
	}
	constexpr const DxcDefine* defines = nullptr;

	IDxcCompilerArgs* compileArguments = nullptr;
	CHECK_RESULT(Utils->BuildArguments(pathBuffer, entryPoint, profile, arguments, ARRAY_COUNT(arguments), defines, 0, &compileArguments));

	const DxcBuffer buffer =
	{
		.Ptr = static_cast<uint8*>(sourceBlob->GetBufferPointer()),
		.Size = sourceBlob->GetBufferSize(),
		.Encoding = DXC_CP_UTF8,
	};
	IDxcResult* compileResult = nullptr;
	dxcResult = Compiler->Compile(&buffer, compileArguments->GetArguments(), compileArguments->GetCount(), nullptr, IID_PPV_ARGS(&compileResult));
	CHECK(SUCCEEDED(dxcResult) && compileResult);

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

D3D12Shader::D3D12Shader(const Shader& shader)
{
	IDxcResult* compileResult = Dxc::CompileShader(shader.GetStage(), shader.GetFilePath());

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
