#include "PCH.h"
#include "FMessageHandler.h"

#include "../../ErrorHandler.h"

bool FMessageHandler::Handles(const FTypeInfo* Type) const noexcept {
	return mMessageType->isExactlyA(Type);
}

const FTypeInfo& FMessageHandler::GetMessageType() const noexcept {
    if (mMessageType == nullptr) {
        ErrorHandler::Report("FMessageHandler::GetMessageType", "The message handler has no registered message type.", ErrorHandler::EErrorLevel::Critical);
    }

    return *mMessageType;
}

void FMessageHandler::Invoke(const FMessage& Message) {
    if (!mFunction) {
        ErrorHandler::Report("FMessageHandler::Invoke", "The message handler has no callable function.", ErrorHandler::EErrorLevel::Critical);
    }

    mFunction(Message);
}
