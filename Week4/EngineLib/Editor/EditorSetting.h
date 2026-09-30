#pragma once

#include "Core/Core.h"
#include "../Core/Math/MathUtility.h"
#include "ViewportTypes.h"
#include "../Core/Container/TArray.h"
#include <fstream>

struct FViewportSetting
{
	EViewportType Type = EViewportType::Perspective;
	EViewModeIndex ViewMode = EViewModeIndex::VMI_Lit;

	uint32 ShowFlags =
		static_cast<uint32>(EEngineShowFlags::SF_Primitives) |
		static_cast<uint32>(EEngineShowFlags::SF_BillboardText) |
		static_cast<uint32>(EEngineShowFlags::SF_WorldAxis) |
		static_cast<uint32>(EEngineShowFlags::SF_BoundingBox) |
		static_cast<uint32>(EEngineShowFlags::SF_Grid);
};

struct FEditorSetting
{
	float CameraSensitivity = 0.1f;
	float GridSpacing = 0.5f;

	uint8 viewportCount = 1;

	float twoPaneVertical = 0.5f;

	float threePaneVertical = 0.5f;
	float threePaneRightHorizontal = 0.5;

	float fourPaneHorizontal = 0.5f;
	float fourPaneVertical = 0.5f;

	uint8 maxViewportCount = 4;

	TArray<FViewportSetting> Viewports = { 
		{ EViewportType::Perspective },
		{ EViewportType::Top },
		{ EViewportType::Front },
		{ EViewportType::Right }
	};

	void Load(const std::string& FilePath = "Config/editor.ini")
	{
		std::ifstream File(FilePath);

		if (!File.is_open())
		{
			throw std::runtime_error("Load failed");
		}

		std::string Key;

		while (File >> Key)
		{
			if (Key == "CameraSensitivity")
			{
				File >> CameraSensitivity;
			}
			if (Key == "GridSpacing")
			{
				File >> GridSpacing;
			}
			if (Key == "ViewportCount")
			{
				int32 loadedViewportCount = 1;
				File >> loadedViewportCount;
				viewportCount = static_cast<uint8>(FMath::Clamp(loadedViewportCount,1,static_cast<int32>(maxViewportCount)));
			}
			if (Key == "TwoPaneVertical")
			{
				File >> twoPaneVertical;
			}
			if (Key == "ThreePaneVertical")
			{
				File >> threePaneVertical;
			}
			if (Key == "ThreePaneRightHorizontal")
			{
				File >> threePaneRightHorizontal;
			}
			if (Key == "FourPaneHorizontal")
			{
				File >> fourPaneHorizontal;
			}
			if (Key == "FourPaneVertical")
			{
				File >> fourPaneVertical;
			}
			for (uint8 index = 0; index < maxViewportCount; ++index)
			{
				const std::string prefix = "Viewport" + std::to_string(index);

				if (Key == prefix + "Type")
				{
					int32 value = 0;
					File >> value;

					if (value >= static_cast<int32>(EViewportType::Perspective) &&
						value <= static_cast<int32>(EViewportType::Back))
					{
						Viewports[index].Type = static_cast<EViewportType>(value);
					}
				}
				else if (Key == prefix + "ViewMode")
				{
					uint32 value = 0;
					File >> value;

					if (value <= static_cast<uint32>(EViewModeIndex::VMI_Wireframe))
					{
						Viewports[index].ViewMode = static_cast<EViewModeIndex>(value);
					}
				}
				else if (Key == prefix + "ShowFlags")
				{
					File >> Viewports[index].ShowFlags;
				}
			}
		}

	}

	void Save(const std::string& FilePath = "Config/editor.ini") const
	{
		std::ofstream File(FilePath);

		if (!File.is_open())
		{
			throw std::runtime_error("Save failed");
		}

		File << "CameraSensitivity " << CameraSensitivity << '\n';
		File << "GridSpacing " << GridSpacing << '\n';
		File << "ViewportCount " << static_cast<int32>(viewportCount) << '\n';
		File << "TwoPaneVertical " << twoPaneVertical << '\n';
		File << "ThreePaneVertical " << threePaneVertical << '\n';
		File << "ThreePaneRightHorizontal " << threePaneRightHorizontal << '\n';
		File << "FourPaneHorizontal " << fourPaneHorizontal << '\n';
		File << "FourPaneVertical " << fourPaneVertical << '\n';

		for (uint8 index = 0; index < maxViewportCount; ++index)
		{
			const FViewportSetting& viewport = Viewports[index];

			File << "Viewport" << static_cast<int32>(index)<< "Type "
				<< static_cast<int32>(viewport.Type)<< '\n';
			File << "Viewport" << static_cast<int32>(index)<< "ViewMode "
				<< static_cast<uint32>(viewport.ViewMode)<< '\n';
			File << "Viewport" << static_cast<int32>(index)<< "ShowFlags "
				<< viewport.ShowFlags << '\n';
		}
	}
};
