#pragma once

#include "Luft/Base.hpp"

#define PAD(size) char TOKEN_PASTE(Pad, __LINE__) [(size)]

struct Float2
{
	float X;
	float Y;
};

struct Float3
{
	float X;
	float Y;
	float Z;
};

struct Float4
{
	float X;
	float Y;
	float Z;
	float W;
};
