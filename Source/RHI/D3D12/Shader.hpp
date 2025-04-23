#pragma once

#include "Base.hpp"

#include "RHI/Shader.hpp"

#include "Luft/HashTable.hpp"

namespace RHI::D3D12
{

class Shader final : public ShaderDescription, NoCopy
{
public:
	Shader(const ShaderDescription& description, Device* device);
	~Shader();

	IDxcBlob* Blob;
	ID3D12ShaderReflection* Reflection;
	Device* Device;
};

}

namespace Dxc
{

struct RootParameter
{
	usize Index;
	usize Size;
};

void Init();
void Shutdown();

void ReflectInputElements(ID3D12ShaderReflection* shaderReflection, Array<D3D12_INPUT_ELEMENT_DESC>& inputElements);
void ReflectRootParameters(ID3D12ShaderReflection* shaderReflection,
						   HashTable<String, RootParameter>* rootParameters,
						   HashTable<String, D3D12_ROOT_PARAMETER1>* apiRootParameters);

}
