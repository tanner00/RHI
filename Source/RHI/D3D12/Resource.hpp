#pragma once

#include "Device.hpp"

#include "RHI/Resource.hpp"

namespace RHI::D3D12
{

class Resource final : public ResourceDescription, NoCopy
{
public:
	Resource(const ResourceDescription& description, Device* device);
	~Resource();

	void Write(const ResourceDescription& format, const void* data);
	void WriteBuffer(const ResourceDescription& format, const void* data);
	void WriteTexture(const ResourceDescription& format, const void* data);
	void WriteAccelerationStructureInstances(const ResourceDescription& format, const void* data);

	void Copy(ID3D12GraphicsCommandList10* commandList, ID3D12Resource2* source) const;
	void CopyBuffer(ID3D12GraphicsCommandList10* commandList, ID3D12Resource2* source) const;
	void CopyTexture(ID3D12GraphicsCommandList10* commandList, ID3D12Resource2* source) const;
	void CopyAccelerationStructureInstances(ID3D12GraphicsCommandList10* commandList, ID3D12Resource2* source) const;

	ID3D12Resource2* Native;
	Device* Device;
};

}
