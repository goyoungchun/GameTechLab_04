#include "PCH.h"
#include "FMessage.h"

#include "../../ErrorHandler.h"

FMessage::FMessage(FMessage&& Other) noexcept {
	FMessage::MoveFrom(std::move(Other));
}

FMessage& FMessage::operator=(FMessage&& Other) noexcept {
	if (this != &Other) {
		FMessage::Reset();
		FMessage::MoveFrom(std::move(Other));
	}

	return *this; 
}

FMessage::~FMessage() noexcept {
	FMessage::Reset(); 
}

bool FMessage::IsValid() const noexcept {
	return mData != nullptr;
}

const FTypeInfo* FMessage::GetTypeInfo() const noexcept {
	if (mType == nullptr) {
		ErrorHandler::Report("FMessage::GetTypeInfo", "Cannot get type information from an invalid message.", ErrorHandler::EErrorLevel::Critical);
	}

	return mType;
}

void FMessage::Reset() noexcept {
	if (mData != nullptr) {
		if (mDestroyFunction == nullptr) {
			ErrorHandler::Report("FMessage::Reset", "A valid message does not have a destroy function.", ErrorHandler::EErrorLevel::Critical);
		}

		mDestroyFunction(*this);
	}

	ClearMetadata();
}

void FMessage::MoveFrom(FMessage&& Other) noexcept {
    if (!Other.IsValid()) {
        return;
    }

    mType = Other.mType;
    mDestroyFunction = Other.mDestroyFunction;
    mMoveFunction = Other.mMoveFunction;
    mHeapAllocated = Other.mHeapAllocated;

    if (mHeapAllocated) {
        mData = std::exchange(Other.mData, nullptr);
        Other.ClearMetadata();
        return;
    }

    if (mMoveFunction == nullptr) {
        ErrorHandler::Report("FMessage::MoveFrom", "An inline message does not have a move function.", ErrorHandler::EErrorLevel::Critical);
    }

    mMoveFunction(*this, Other);
    Other.ClearMetadata();
}

void FMessage::ClearMetadata() noexcept {
	mType = nullptr;
	mData = nullptr;
	mDestroyFunction = nullptr;
	mMoveFunction = nullptr;
	mHeapAllocated = false;
}








