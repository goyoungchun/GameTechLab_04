#include "PCH.h"
#include "FUndoTransaction.h"

#include "IUndoRecord.h"
#include <ranges>

FUndoTransaction::~FUndoTransaction() = default;

void FUndoTransaction::Undo(IUndoContext& Context)
{
    for (const auto& Record : std::views::reverse(Records))
    {
        Record->ApplyUndo(Context);
    }
}

void FUndoTransaction::Redo(IUndoContext& Context)
{
    for (const auto& Record : Records)
    {
        Record->ApplyRedo(Context);
    }
}