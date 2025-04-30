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

	void Write(const void* data, Array<UploadPair<ID3D12Resource2*, Resource*>>* pendingUploads);
	void WriteBuffer(const void* data, Array<UploadPair<ID3D12Resource2*, Resource*>>* pendingUploads);
	void WriteTexture(const void* data, Array<UploadPair<ID3D12Resource2*, Resource*>>* pendingUploads);
	void WriteAccelerationStructureInstances(const void* data);

	void Upload(ID3D12GraphicsCommandList10* uploadCommandList, ID3D12Resource2* source) const;
	void UploadBuffer(ID3D12GraphicsCommandList10* uploadCommandList, ID3D12Resource2* source) const;
	void UploadTexture(ID3D12GraphicsCommandList10* uploadCommandList, ID3D12Resource2* source) const;
	void UploadAccelerationStructureInstances(ID3D12GraphicsCommandList10* uploadCommandList, ID3D12Resource2* source) const;

	ID3D12Resource2* Native;
	Device* Device;
};

}
