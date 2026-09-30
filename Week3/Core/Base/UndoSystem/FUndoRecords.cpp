#include "PCH.h"
#include "FUndoRecords.h"
#include "IUndoContext.h"

void FRecordObjectState::ApplyUndo(IUndoContext& Context)
{
    Context.NotifyObjectChanged(TargetGuid, BeforeData);
}
void FRecordObjectState::ApplyRedo(IUndoContext& Context)
{
    Context.NotifyObjectChanged(TargetGuid, AfterData);
}


void FRecordObjectSpawned::ApplyUndo(IUndoContext& Context)
{
    // 생성의 취소는 곧 삭제
    Context.NotifyObjectDeleted(TargetGuid);
}
void FRecordObjectSpawned::ApplyRedo(IUndoContext& Context)
{
    // 다시 생성 (저장해둔 데이터로 부활)
    Context.NotifyObjectSpawned(TargetGuid, SavedData, std::move(TargetTypeName));
}


void FRecordObjectDestroyed::ApplyUndo(IUndoContext& Context) 
{
    // 삭제의 취소는 곧 부활 (저장해둔 죽기 직전 데이터로 부활)
    Context.NotifyObjectSpawned(TargetGuid, SavedData, std::move(TargetTypeName));
}
void FRecordObjectDestroyed::ApplyRedo(IUndoContext& Context)
{
    // 다시 삭제
    Context.NotifyObjectDeleted(TargetGuid);
}
