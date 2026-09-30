#pragma once

#include "Container/FString.h"

namespace File
{
	void WriteText(FStringView Path, FStringView Text);
	FString ReadText(FStringView Path);
};