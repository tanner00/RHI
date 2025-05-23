#pragma once

#include "Luft/Base.hpp"

#define PAD(size) char TOKEN_PASTE(Pad, __LINE__) [(size)]

struct Float2
{
	union
	{
		float X;
		float R;
	};
	union
	{
		float Y;
		float G;
	};
};

struct Float3
{
	union
	{
		float X;
		float R;
	};
	union
	{
		float Y;
		float G;
	};
	union
	{
		float Z;
		float B;
	};
};

struct Float4
{
	union
	{
		float X;
		float R;
	};
	union
	{
		float Y;
		float G;
	};
	union
	{
		float Z;
		float B;
	};
	union
	{
		float W;
		float A;
	};
};
