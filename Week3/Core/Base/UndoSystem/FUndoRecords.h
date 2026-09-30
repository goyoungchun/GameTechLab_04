#pragma once
#include "IUndoRecord.h" 
#include "../FGuid.h"    

class FRecordObjectState: public IUndoRecord
{
public:
    FRecordObjectState(const FGuid& InGuid, const TArray<uint8>& InBefore, const TArray<uint8>& InAfter)
        : TargetGuid(InGuid), BeforeData(InBefore), AfterData(InAfter) {}

    virtual void ApplyUndo(IUndoContext& Context) override;
    virtual void ApplyRedo(IUndoContext& Context) override;

private:
    FGuid TargetGuid;

    TArray<uint8> BeforeData;
    TArray<uint8> AfterData;
};


class FRecordObjectSpawned : public IUndoRecord
{
public:
    FRecordObjectSpawned(FGuid InputGuid, const TArray<uint8>& InputSavedData, std::string_view InputTargetTypeName)
        : TargetGuid(InputGuid), SavedData(std::move(InputSavedData)), TargetTypeName(InputTargetTypeName){}
    
    virtual void ApplyUndo(IUndoContext& Context) override;
    virtual void ApplyRedo(IUndoContext& Context) override;
private:
    FGuid TargetGuid;
    TArray<uint8> SavedData;
    FString TargetTypeName;
};


class FRecordObjectDestroyed : public IUndoRecord
{
public:
    FRecordObjectDestroyed(FGuid InputGuid, TArray<uint8> InputSavedData, std::string_view InputTargetTypeName)
        : TargetGuid(InputGuid), SavedData(std::move(InputSavedData)), TargetTypeName(InputTargetTypeName) {}

    virtual void ApplyUndo(IUndoContext& Context) override;
    virtual void ApplyRedo(IUndoContext& Context) override;
private:
    FGuid TargetGuid;
    TArray<uint8> SavedData;
    FString TargetTypeName;
};