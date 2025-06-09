#pragma once

#include "Forward.hpp"
#include "Resource.hpp"
#include "View.hpp"

namespace RHI
{

struct TextureViewDescription
{
	Resource Resource;
	ViewType Type;
};

class TextureView final : public TextureViewDescription
{
public:
	TextureView()
		: TextureViewDescription()
		, Backend(nullptr)
	{
	}

	TextureView(const TextureViewDescription& description, RHI_BACKEND(TextureView)* backend)
		: TextureViewDescription(description)
		, Backend(backend)
	{
	}

	static TextureView Invalid() { return {}; }
	bool IsValid() const { return Backend != nullptr; }

	RHI_BACKEND(TextureView)* Backend;
};

}
