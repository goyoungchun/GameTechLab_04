#pragma once
#include "../FGuid.h" 
#include "../../Base/TypeInfo.h"
#include "../../../Common.h"

struct FMessageUndoApply
{
    JG_DECLARE_CHANNEL_MESSAGE(FMessageUndoApply); 

    bool bIsUndo; // true = undo, false = redo

    FMessageUndoApply(const bool InputType) noexcept
        : bIsUndo(InputType){}
};


struct FMessageUndoObjectStateChanged
{
    JG_DECLARE_CHANNEL_MESSAGE(FMessageUndoObjectStateChanged);

    FGuid TargetGuid;
    TArray<uint8> SavedData;

    FMessageUndoObjectStateChanged(const FGuid& InputGuid, TArray<uint8>&& InputData) noexcept
        : TargetGuid(InputGuid), SavedData(std::move(InputData)) {}
};


struct FMessageUndoObjectSpawned
{
    JG_DECLARE_CHANNEL_MESSAGE(FMessageUndoObjectSpawned);

    FGuid TargetGuid;
    TArray<uint8> SavedData;
    FString TargetTypeName;

    FMessageUndoObjectSpawned(const FGuid& InputGuid, TArray<uint8>&& InputData, FString&& InputTargetTypeName) noexcept
        : TargetGuid(InputGuid), SavedData(std::move(InputData)), TargetTypeName(InputTargetTypeName) {}
};


struct FMessageUndoObjectDestroyed
{
    JG_DECLARE_CHANNEL_MESSAGE(FMessageUndoObjectDestroyed);

    FGuid TargetGuid;

    FMessageUndoObjectDestroyed(const FGuid& InputGuid) noexcept
        : TargetGuid(InputGuid) {}
};