#include "Pipeline.hpp"
#include "Device.hpp"

namespace RHI::D3D12
{

Pipeline::~Pipeline()
{
	Device->AddPendingDelete(RootSignature);
	Device->AddPendingDelete(PipelineState);
}

}
