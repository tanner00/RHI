#pragma once

#include "Forward.hpp"

#include "Luft/Base.hpp"

namespace RHI
{

enum class HeapType : uint8
{
	Default,
	Upload,
	ReadBack,
};

struct HeapDescription
{
	HeapType Type;
	usize Size;
};

class Heap final : public HeapDescription
{
public:
	Heap()
		: HeapDescription()
		, Backend(nullptr)
	{
	}

	Heap(const HeapDescription& description, RHI_BACKEND(Heap)* backend)
		: HeapDescription(description)
		, Backend(backend)
	{
	}

	static Heap Invalid() { return {}; }
	bool IsValid() const { return Backend != nullptr; }

	RHI_BACKEND(Heap)* Backend;
};

}
