#include "PCH.h"
#include "FMessageChannel.h"

FMessageChannel::FSender FMessageChannel::GetSender() noexcept {
	return FMessageChannel::FSender{ *this };
}

FMessageDispatchResult FMessageChannel::Dispatch() {
    if (IsDispatching) {
        RedispatchRequested = true;
        return { .Deferred = true };
    }

    struct FDispatchScope {
        explicit FDispatchScope(bool& InIsDispatching) noexcept : IsDispatching(InIsDispatching) {
            IsDispatching = true;
        }

        ~FDispatchScope() noexcept {
            IsDispatching = false;
        }

        bool& IsDispatching;
    };

    FDispatchScope Scope{ IsDispatching };
    FMessageDispatchResult Result;

    do {
        RedispatchRequested = false;

        const std::size_t DispatchCount = Messages.size();

        for (std::size_t Index = 0; Index < DispatchCount; ++Index) {
            FMessage Message = std::move(Messages.front());
            Messages.pop_front();

            FMessageHandler* Handler = FindHandler(Message.GetTypeInfo());

            if (Handler == nullptr) {
                ++Result.UnhandledCount;
                continue;
            }

            Handler->Invoke(Message);
            ++Result.DispatchedCount;
        }

        CommitPendingHandlers();
    } while (RedispatchRequested);

    return Result;
}

void FMessageChannel::Clear() noexcept {
	Messages.clear();
}

bool FMessageChannel::IsEmpty() const noexcept {
	return Messages.empty();
}

bool FMessageChannel::IsFull() const noexcept {
	return Capacity != 0 && Messages.size() >= Capacity;
}

std::size_t FMessageChannel::Size() const noexcept {
	return Messages.size();
}

std::size_t FMessageChannel::GetCapacity() const noexcept {
	return Capacity;
}

FMessageHandler* FMessageChannel::FindHandler(const FTypeInfo* Type) noexcept {
	const auto Iterator = std::ranges::find_if(Handlers, [&Type](const FMessageHandler& Handler)
		{
			return Handler.Handles(Type);
		});

	return Iterator != Handlers.end() ? &(*Iterator) : nullptr;
}

FMessageHandler* FMessageChannel::FindPendingHandler(const FTypeInfo* Type) noexcept {
	const auto Iterator = std::ranges::find_if(PendingHandlers, [&Type](const FMessageHandler& Handler)
		{
			return Handler.Handles(Type);
		});

	return Iterator != PendingHandlers.end() ? &(*Iterator) : nullptr;
}

void FMessageChannel::CommitPendingHandlers() {
	if (PendingHandlers.empty()) {
		return;
	}

	Handlers.reserve(Handlers.size() + PendingHandlers.size());

	for (FMessageHandler& Handler : PendingHandlers) {
		Handlers.emplace_back(std::move(Handler));
	}

	PendingHandlers.clear();
}
