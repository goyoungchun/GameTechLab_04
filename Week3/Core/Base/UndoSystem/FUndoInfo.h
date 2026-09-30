#pragma once
#include "../FGuid.h" 
#include "../../Base/TypeInfo.h"

struct FObjectStateChangedMessage
{
public:
    FGuid TargetGuid;
    std::vector<uint8> StateData;


    inline static const FTypeInfo TypeInfo{
        "FObjectStateChangedMessage",
        nullptr,
        nullptr
    };
    static const FTypeInfo& StaticTypeInfo() noexcept { return TypeInfo; }


    FObjectStateChangedMessage(const FGuid& InGuid, const std::vector<uint8>& InData)
        : TargetGuid(InGuid), StateData(InData)
    {
    }

    FObjectStateChangedMessage(const FGuid& InGuid, std::vector<uint8>&& InData) noexcept
        : TargetGuid(InGuid), StateData(std::move(InData))
    {
    }

    FObjectStateChangedMessage() = default;
    ~FObjectStateChangedMessage() = default;
    FObjectStateChangedMessage(const FObjectStateChangedMessage&) = default;
    FObjectStateChangedMessage& operator=(const FObjectStateChangedMessage&) = default;
    FObjectStateChangedMessage(FObjectStateChangedMessage&&) noexcept = default;
    FObjectStateChangedMessage& operator=(FObjectStateChangedMessage&&) noexcept = default;
};