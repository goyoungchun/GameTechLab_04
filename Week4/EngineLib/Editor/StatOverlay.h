#pragma once

struct FRect;
class FStatManager;
struct ImDrawList;

namespace StatOverlay
{
	void Draw(const FRect& imageRect,const FStatManager& statManager,ImDrawList& drawList);
}
