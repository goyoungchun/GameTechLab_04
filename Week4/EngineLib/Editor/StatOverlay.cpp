#include "../Editor/StatOverlay.h"

#include <algorithm>
#include <cstdio>

#include "Editor/ViewportTypes.h"
#include "Engine/Stats/StatManager.h"
#include "ThirdParty/ImGui/imgui.h"

namespace
{
	double ToMiB(uint64 bytes)
	{
		return static_cast<double>(bytes) / (1024.0 * 1024.0);
	}

	void DrawFps(const FRect& rect,const FFrameStatSnapshot& frame,ImDrawList& drawList)
	{
		char fpsText[64];
		char timeText[64];

		std::snprintf(fpsText,sizeof(fpsText),"%.1f FPS",frame.SmoothedFPS);
		std::snprintf(timeText,sizeof(timeText),"%.2f ms",frame.SmoothedFrameTimeMs);

		const ImVec2 fpsSize = ImGui::CalcTextSize(fpsText);
		const ImVec2 timeSize =	ImGui::CalcTextSize(timeText);
		const float width = (std::max)(fpsSize.x, timeSize.x) + 16.0f;
		const float height = fpsSize.y + timeSize.y + 12.0f;
		const ImVec2 min(rect.Right - width - 12.0f,rect.Top + 12.0f);
		const ImVec2 max(min.x + width,min.y + height);

		drawList.AddRectFilled(min,max,IM_COL32(0, 0, 0, 150),4.0f);
		drawList.AddText(ImVec2(max.x - fpsSize.x - 8.0f,min.y + 4.0f),IM_COL32(100, 255, 120, 255),fpsText);
		drawList.AddText(ImVec2(max.x - timeSize.x - 8.0f,min.y + fpsSize.y + 4.0f),IM_COL32(220, 220, 220, 255),timeText);
	}
}

namespace
{
	void DrawMemory(const FRect& rect,const FMemoryStatSnapshot& memory,ImDrawList& drawList)
	{
		char lines[12][96];
		int lineCount = 0;

		std::snprintf(lines[lineCount++],96,"Memory");
		std::snprintf(lines[lineCount++],96,"Private: %.1f MiB",ToMiB(memory.PrivateBytes));
		std::snprintf(lines[lineCount++],96,"Working Set: %.1f MiB",ToMiB(memory.WorkingSetBytes));
		std::snprintf(lines[lineCount++],96,"Peak Working: %.1f MiB",ToMiB(memory.PeakWorkingSetBytes));
		std::snprintf(lines[lineCount++],96,"Tracked C++: %.1f MiB / %llu allocs",ToMiB(memory.TrackedAllocationBytes),
			static_cast<unsigned long long>(memory.TrackedAllocationCount));
		std::snprintf(lines[lineCount++],96,
			"Object: %u / Actor: %u / Component: %u",
			memory.UObjectCount,
			memory.ActorCount,
			memory.ComponentCount);

		std::snprintf(lines[lineCount++],96,
			"RenderInfo: %u",
			memory.RenderInfoCount);

		std::snprintf(lines[lineCount++],96,
			"Mesh: %u / Material: %u",
			memory.StaticMeshCount,
			memory.MaterialCount);

		std::snprintf(lines[lineCount++],96,
			"GPU Buffer: %u / Texture: %u",
			memory.GpuBufferCount,
			memory.GpuTextureCount);

		float width = 0.0f;
		const float lineHeight = ImGui::GetTextLineHeight();

		for (int index = 0; index < lineCount; ++index)
		{
			width = (std::max)(width,ImGui::CalcTextSize(lines[index]).x);
		}

		const ImVec2 min(rect.Left + 12.0f,rect.Top + 12.0f);
		const ImVec2 max(min.x + width + 16.0f,min.y + lineHeight * lineCount + 12.0f);

		drawList.AddRectFilled(min,max,IM_COL32(0, 0, 0, 170),4.0f);

		for (int index = 0; index < lineCount; ++index)
		{
			const ImU32 color =index == 0? IM_COL32(255, 170, 40, 255): IM_COL32(100, 255, 120, 255);
			drawList.AddText(ImVec2(min.x + 8.0f,min.y + 6.0f +lineHeight * index),color,lines[index]);
		}
	}
}

void StatOverlay::Draw(const FRect& imageRect,const FStatManager& statManager,ImDrawList& drawList)
{
	if (!statManager.HasAnyEnabledStat())
	{
		return;
	}

	const FStatSnapshot& snapshot = statManager.GetSnapshot();

	drawList.PushClipRect(ImVec2(imageRect.Left, imageRect.Top),ImVec2(imageRect.Right, imageRect.Bottom),true);
	if (statManager.IsEnabled(EStatGroup::FPS))
	{
		DrawFps(imageRect,snapshot.Frame,drawList);
	}

	if (statManager.IsEnabled(EStatGroup::Memory))
	{
		DrawMemory(imageRect,snapshot.Memory,drawList);
	}
	drawList.PopClipRect();
}
