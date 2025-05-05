#pragma once

#include "Buffer.hpp"
#include "Forward.hpp"
#include "View.hpp"

namespace RHI
{

struct BufferViewDescription
{
	ViewType Type;
	Buffer Buffer;
};

class BufferView final : public BufferViewDescription
{
public:
	BufferView()
		: BufferViewDescription()
		, Backend(nullptr)
	{
	}

	BufferView(const BufferViewDescription& description, RHI_BACKEND(BufferView)* backend)
		: BufferViewDescription(description)
		, Backend(backend)
	{
	}

	static BufferView Invalid() { return {}; }
	bool IsValid() const { return Backend != nullptr; }

	RHI_BACKEND(BufferView)* Backend;
};

}
