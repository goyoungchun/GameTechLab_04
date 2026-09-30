#pragma once

class IUndoContext
{
public:
    virtual ~IUndoContext() = default;

    virtual void NotifyObjectChanged(const FGuid&, const TArray<uint8>&) = 0;
    virtual void NotifyObjectSpawned(const FGuid&, const TArray<uint8>&, FString&& TypeName) = 0;
    virtual void NotifyObjectDeleted(const FGuid&) = 0;
};