#include "Pipeline.hpp"
#include "Device.hpp"

namespace RHI::D3D12
{

Pipeline::~Pipeline()
{
	SAFE_RELEASE(RootSignature);
	SAFE_RELEASE(PipelineState);
}

}
