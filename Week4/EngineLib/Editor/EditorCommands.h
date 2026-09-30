#pragma once


#include <variant>

#include "Core/Core.h"
#include "Core/Container/TArray.h"
#include "Core/enum.h"
#include "Core/Name.h"
#include "Core/Object/Object.h"
#include "Core/PropertyEnum.h"
#include "Core/Math/Vector.h"
#include "Core/Math/Rotator.h"
#include "Core/Math/Color.h"

#include "Editor/ViewportTypes.h"
#include "Engine/Stats/StatTypes.h"

#include <variant>

/* Editor Commands */
/* SceneManager Commands */
struct FNewSceneCommand {};
struct FSaveSceneCommand { FString SceneName; };
struct FSaveSceneAsCommand { FString SceneName; };
struct FLoadSceneCommand { FString SceneName; };
struct FLoadObjCommand { FString ObjFilePath; };

struct FSpawnActorCommand { EPrimitive PrimitiveType; int32 SpawnCount; };
struct FSpawnStaticMeshActorCommand { FName StaticMeshKey; int32 SpawnCount; };
struct FDeleteActorCommand { FObjectID ObjectID; };
struct FSpawnParticleCommand { };

struct FSetActorLocationCommand { FObjectID ObjectID; FVector Location; };
struct FSetActorRotationCommand { FObjectID ObjectID; FRotator Rotation; };
struct FSetActorScaleCommand { FObjectID ObjectID; FVector Scale; };
struct FSetActorNameCommand { FObjectID ObjectID; FName NewName; };
struct FSetSelectedActorCommand { FObjectID ObjectID; };

struct FSetComponentUseTextureCommand { FObjectID ObjectID; bool bUseTexture; };
struct FSetComponentColorCommand { FObjectID ObjectID; FLinearColor Color; };
struct FSetSphereComponentSpinCommand { FObjectID ObjectID; bool bSpin; };
struct FSetSphereComponentSpinSpeedCommand { FObjectID ObjectID; float SpinSpeed; };
struct FSetStaticMeshComponentStaticMeshCommand { FObjectID ObjectID;	FName StaticMeshAssetKey; };
struct FSetStaticMeshComponentMaterialCommand { FObjectID ObjectID; int32 MaterialSlotIndex; FName MaterialAssetKey; };
struct FSetStaticMeshComponentSubUVCommand { FObjectID ObjectID; FVector2 UVOffset; FVector2 UVScale; };
struct FSetParticleSubUVComponentLoopingCommand { FObjectID ObjectID; bool bLooping; };
struct FSetParticleSubUVComponentPlayRateCommand { FObjectID ObjectID; float PlayRate; };
struct FSetParticleSubUVComponentBlendStateTypeCommand { FObjectID ObjectID; EBlendStateType BlendStateType; };
struct FSetPropertyCommand { FObjectID ObjectID; FString PropertyName; FPropertyValue NewValue; };

/* EditorViewportClient Commands */
struct FSetViewModeCommand { EViewModeIndex ViewMode; };
struct FSetShowFlagCommand { uint32 ShowFlags; };
struct FSetCameraSensitivityCommand { float Sensitivity; };
struct FSetCameraFovCommand { float Fov; };
struct FSetCameraLocationCommand { FVector Location; };
struct FSetCameraRotationCommand { FRotator Rotation; };
struct FSetGizmoModeCommand { EGIZMO_TYPE GizmoMode; };
struct FCycleGizmoModeCommand {};

struct FSetViewportTypeCommand { uint8 viewportId; EViewportType Type; };
struct FSetViewportViewModeCommand { uint8 viewportId; EViewModeIndex ViewMode; };
struct FSetViewportShowFlagCommand { uint8 viewportId; EEngineShowFlags Flag; bool bEnabled; };
struct FSetSharedCameraSpeedCommand { float Speed; };
struct FSetSharedSnapPresetCommand { uint8 PresetIndex; };
struct FSetViewportFovCommand { uint8 ViewportId; float Fov; };

/* GraphicsManager Commands */
struct FSetGridWidthCommand { float GridWidth; };
struct FStartProjectionTransitionCommand { bool bOrthographic; };

/* Consol Commands */
struct FToggleStatCommand { EStatGroup Group; };
struct FDisableAllStatsCommand { };

using FEditorCommand = std::variant <
	FNewSceneCommand,
	FSaveSceneCommand,
	FSaveSceneAsCommand,
	FLoadSceneCommand,
	FLoadObjCommand,

	FSpawnActorCommand,
	FDeleteActorCommand,
	FSpawnParticleCommand,
	FSpawnStaticMeshActorCommand,

	FSetActorLocationCommand,
	FSetActorRotationCommand,
	FSetActorScaleCommand,
	FSetActorNameCommand,
	FSetSelectedActorCommand,

	FSetComponentUseTextureCommand,
	FSetComponentColorCommand,
	FSetStaticMeshComponentStaticMeshCommand,
	FSetStaticMeshComponentMaterialCommand,
	FSetStaticMeshComponentSubUVCommand,
	FSetSphereComponentSpinCommand,
	FSetSphereComponentSpinSpeedCommand,
	FSetParticleSubUVComponentLoopingCommand,
	FSetParticleSubUVComponentPlayRateCommand,
	FSetParticleSubUVComponentBlendStateTypeCommand,
	FSetPropertyCommand,

	FSetViewModeCommand,
	FSetShowFlagCommand,
	FSetCameraSensitivityCommand,
	FSetCameraFovCommand,
	FSetCameraLocationCommand,
	FSetCameraRotationCommand,
	FSetGizmoModeCommand,
	FCycleGizmoModeCommand,

	FSetViewportTypeCommand,
	FSetViewportViewModeCommand,
	FSetViewportShowFlagCommand,
	FSetSharedCameraSpeedCommand,
	FSetSharedSnapPresetCommand,
	FSetViewportFovCommand,

	FSetGridWidthCommand,
	FStartProjectionTransitionCommand,

	FToggleStatCommand,
	FDisableAllStatsCommand

> ;
using FEditorCommands = TArray<FEditorCommand>;
