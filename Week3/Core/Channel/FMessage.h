#pragma once

#include <concepts>
#include <cstddef>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

#include "../Base/TypeInfo.h"
#include "../Memory/Memory.h"
// #include "Memory/Memory.h" -- until heap 

// Memory Placeholders
//namespace Memory {
//    inline void* Allocate(std::size_t Size, std::size_t Alignment, int /*EMemoryTag*/) noexcept {
//		return ::operator new(Size, std::align_val_t(Alignment));
//	}
//
//	inline void Free(void* Ptr, std::size_t Alignment) noexcept {
//		::operator delete(Ptr, std::align_val_t(Alignment));
//	}
//
//	enum EMemoryTag {
//		Message
//	};
//}

template<typename T>
concept CMessageType = requires {
    { T::StaticTypeInfo() } -> std::same_as<const FTypeInfo&>;
} && std::is_nothrow_move_constructible_v<T> && std::is_nothrow_destructible_v<T>;

template<typename T, typename... Args>
concept CMessageConstructible = CMessageType<T> && std::is_nothrow_constructible_v<T, Args...>;

class FMessage {
public:
    static constexpr std::size_t InlineSize = 64;
    static constexpr std::size_t InlineAlignment = alignof(std::max_align_t);

private:
    using FDestroyFunction = void (*)(FMessage&) noexcept;
    using FMoveFunction = void (*)(FMessage&, FMessage&) noexcept;

public:
    FMessage() noexcept = default;

    FMessage(const FMessage&) = delete;
    FMessage& operator=(const FMessage&) = delete;

    FMessage(FMessage&& Other) noexcept;
    FMessage& operator=(FMessage&& Other) noexcept;

    ~FMessage() noexcept;

public:
    template<typename T, typename... Args> requires CMessageConstructible<T, Args...>
    inline void TryEmplace(Args&&... Arguments) {
        FMessage::Reset();

        if constexpr (CanStoreInline<T>()) {
            T* Value = std::construct_at(reinterpret_cast<T*>(mInlineStorage), std::forward<Args>(Arguments)...);

            mType = &T::StaticTypeInfo();
            mData = Value;
            mDestroyFunction = &FMessage::DestroyInline<T>;
            mMoveFunction = &FMessage::MoveInline<T>;
            mHeapAllocated = false;
        }
        else {
            void* Allocation = Memory::Allocate(sizeof(T), alignof(T), Memory::EMemoryTag::Message);
            T* Value = std::construct_at(static_cast<T*>(Allocation), std::forward<Args>(Arguments)...);

            mType = &T::StaticTypeInfo();
            mData = Value;
            mDestroyFunction = &FMessage::DestroyHeap<T>;
            // 포인터 교체만 하면 되니까 움직일 필요가 없다. 
            mMoveFunction = nullptr;
            mHeapAllocated = true;
        }
    }

    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] const FTypeInfo* GetTypeInfo() const noexcept;

    template<CMessageType T>
    [[nodiscard]] inline bool Is() const noexcept {
        return mType == &T::StaticTypeInfo();
    }

    template<CMessageType T>
    [[nodiscard]] inline T* Get() noexcept {
        return Is<T>() ? static_cast<T*>(mData) : nullptr;
    }

    template<CMessageType T>
    [[nodiscard]] inline const T* Get() const noexcept {
        return Is<T>() ? static_cast<const T*>(mData) : nullptr;
    }

    void Reset() noexcept;


private:
    template<typename T>
    static constexpr bool CanStoreInline() noexcept {
        return sizeof(T) <= InlineSize && alignof(T) <= InlineAlignment;
    }

    template<typename T>
    static void DestroyInline(FMessage& Message) noexcept {
        std::destroy_at(static_cast<T*>(Message.mData));
    }

    template<typename T>
    static void DestroyHeap(FMessage& Message) noexcept {
        T* Value = static_cast<T*>(Message.mData);
        std::destroy_at(Value);
        Memory::Free(Value);
    }

    template<typename T>
    static void MoveInline(FMessage& Destination, FMessage& Source) noexcept {
        T* SourceValue = static_cast<T*>(Source.mData);
        T* DestinationValue = std::construct_at(reinterpret_cast<T*>(Destination.mInlineStorage), std::move(*SourceValue));

        std::destroy_at(SourceValue);

        Destination.mData = DestinationValue;
        Source.mData = nullptr;
    }

    void MoveFrom(FMessage&& Other) noexcept;
    void ClearMetadata() noexcept;

private:
    const FTypeInfo* mType{ nullptr };
    void* mData{ nullptr };
    FDestroyFunction mDestroyFunction{ nullptr };
    FMoveFunction mMoveFunction{ nullptr };
    bool mHeapAllocated{ false };

    alignas(InlineAlignment) std::byte mInlineStorage[InlineSize]{};
};
