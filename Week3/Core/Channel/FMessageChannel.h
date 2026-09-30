#pragma once

#include <cstddef>
#include <deque>
#include <ranges>
#include <type_traits>
#include <utility>
#include <vector>

#include "FMessage.h"
#include "FMessageHandler.h"

struct FMessageDispatchResult {
    std::size_t DispatchedCount{};
    std::size_t UnhandledCount{};
    bool Deferred{ false };
};

class FMessageChannel {
public:
    class FSender {
        friend class FMessageChannel;

    public:
        explicit FSender(FMessageChannel& InChannel) noexcept : Channel(&InChannel) {}
        ~FSender() noexcept = default;

        FSender(const FSender&) = default;
        FSender& operator=(const FSender&) = default;

        FSender(FSender&&) = default;
        FSender& operator=(FSender&&) = default;

    public:
        template<CMessageType TMessage, typename... Args> requires CMessageConstructible<TMessage, Args...>
        bool TryEmplace(Args&&... Arguments) {
            return Channel->TryEmplace<TMessage>(std::forward<Args>(Arguments)...);
        }

        template<typename TMessage> requires CMessageType<std::remove_cvref_t<TMessage>>
        bool TryPush(TMessage&& Message) {
            using FMessageType = std::remove_cvref_t<TMessage>;
            return TryEmplace<FMessageType>(std::forward<TMessage>(Message));
        }

    private:
        FMessageChannel* Channel{ nullptr };
    };

public:
    explicit FMessageChannel(std::size_t InCapacity = 0, std::size_t ExpectedHandlerCount = 8) : Capacity(InCapacity) {
        Handlers.reserve(ExpectedHandlerCount);
        PendingHandlers.reserve(ExpectedHandlerCount);
    }

    FMessageChannel(const FMessageChannel&) = delete;
    FMessageChannel& operator=(const FMessageChannel&) = delete;

    FMessageChannel(FMessageChannel&&) = delete;
    FMessageChannel& operator=(FMessageChannel&&) = delete;

public:
    [[nodiscard]] FSender GetSender() noexcept;

    template<CMessageType TMessage, typename TCallable>
        requires std::copy_constructible<std::decay_t<TCallable>>&& std::invocable<std::decay_t<TCallable>&, const TMessage&>
    bool TryBind(TCallable&& Callable) {
        const FTypeInfo& Type = TMessage::StaticTypeInfo();

        if (FindHandler(&Type) != nullptr || FindPendingHandler(&Type) != nullptr) {
            return false;
        }

        FMessageHandler Handler = FMessageHandler::Create<TMessage>(std::forward<TCallable>(Callable));

        if (IsDispatching) {
            PendingHandlers.emplace_back(std::move(Handler));
            return true;
        }

        Handlers.emplace_back(std::move(Handler));
        return true;
    }

    template<CMessageType TMessage, typename... Args> requires CMessageConstructible<TMessage, Args...>
    bool TryEmplace(Args&&... Arguments) {
        if (IsFull()) {
            return false;
        }

        FMessage Message{};
        Message.TryEmplace<TMessage>(std::forward<Args>(Arguments)...);

        Messages.emplace_back(std::move(Message));
        return true;
    }

    template<typename TMessage> requires CMessageType<std::remove_cvref_t<TMessage>>
    bool TryPush(TMessage&& Message) {
        using FMessageType = std::remove_cvref_t<TMessage>;
        return TryEmplace<FMessageType>(std::forward<TMessage>(Message));
    }

    FMessageDispatchResult Dispatch();
    void Clear() noexcept;

    [[nodiscard]] bool IsEmpty() const noexcept;

    [[nodiscard]] bool IsFull() const noexcept;

    [[nodiscard]] std::size_t Size() const noexcept;

    [[nodiscard]] std::size_t GetCapacity() const noexcept;

private:
    FMessageHandler* FindHandler(const FTypeInfo* Type) noexcept;
    FMessageHandler* FindPendingHandler(const FTypeInfo* Type) noexcept;

    void CommitPendingHandlers();

private:
    std::deque<FMessage> Messages{};
    std::vector<FMessageHandler> Handlers{};
    std::vector<FMessageHandler> PendingHandlers{};

    std::size_t Capacity{};

    bool IsDispatching{ false };
    bool RedispatchRequested{ false };
};