#pragma once

#include <concepts>
#include <functional>
#include <type_traits>
#include <utility>

#include "../../ErrorHandler.h"
#include "FMessage.h"

class FMessageHandler
{
public:
    FMessageHandler() = default;
    ~FMessageHandler() noexcept = default;

    FMessageHandler(const FMessageHandler&) = delete;
    FMessageHandler& operator=(const FMessageHandler&) = delete;

    FMessageHandler(FMessageHandler&&) = default;
    FMessageHandler& operator=(FMessageHandler&&) = default;

public:
    template<CMessageType TMessage, typename TCallable>
        requires std::copy_constructible<std::decay_t<TCallable>>&& std::invocable<std::decay_t<TCallable>&, const TMessage&>
    static FMessageHandler Create(TCallable&& Callable) {
        using FCallable = std::decay_t<TCallable>;

        FMessageHandler Result;

        Result.mMessageType = &TMessage::StaticTypeInfo();
        Result.mFunction = [Callable = FCallable(std::forward<TCallable>(Callable))](const FMessage& Message) mutable {
            const TMessage* TypedMessage = Message.Get<TMessage>();
            if (TypedMessage == nullptr) {
                ErrorHandler::Report("FMessageHandler::Create", "The message type does not match the handler's registered type.", ErrorHandler::EErrorLevel::Critical);
            }

            std::invoke(Callable, *TypedMessage);
            };

        return Result;
    }

public:
    [[nodiscard]] bool Handles(const FTypeInfo* Type) const noexcept;
    [[nodiscard]] const FTypeInfo& GetMessageType() const noexcept;

    void Invoke(const FMessage& Message);

private:
    const FTypeInfo* mMessageType{ nullptr };
    std::function<void(const FMessage&)> mFunction{};
};
