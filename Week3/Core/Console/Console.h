#pragma once

#include "pch.h"

enum class ELogLevel { Log, Warning, Error, Fatal };
enum class ELogCategory { Core, Render, Physics, Etc };

struct FConsoleMessage
{
    FString Time;
    ELogLevel Level = ELogLevel::Log;  
    ELogCategory Category = ELogCategory::Etc;
    FString Text;
};

struct FConsoleOutputHandle
{
    uint32_t Index = UINT32_MAX;

    bool IsValid() const
    {
        return Index != UINT32_MAX;
    }
};

namespace Console
{
    inline constexpr FConsoleOutputHandle STDOutHandle{ 0 };
    inline constexpr FConsoleOutputHandle STDErrorHandle{ 1 };

    void Print(FConsoleOutputHandle Handle, FConsoleMessage Message);
    void Clear(FConsoleOutputHandle Handle);
    void AddLog(
        FConsoleOutputHandle Handle,
        ELogLevel Level,
        ELogCategory Categor,
        const char* Format,
        ...);
    void Flush(FConsoleOutputHandle Handle);

    size_t GetMessageCount(FConsoleOutputHandle Handle);
    const FConsoleMessage& GetMessageAt(FConsoleOutputHandle Handle, size_t Index);
}



