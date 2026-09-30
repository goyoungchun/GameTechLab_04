#include "PCH.h"
#include "FArchiveMemory.h"

FArchiveMemory::FArchiveMemory(TArray<uint8>& InBytes)
    : FArchive(EArchiveMode::Saving)
    , WriteBytes(&InBytes)
    , ReadBytes(nullptr)
    , ReadOffset(0)
{
}
FArchiveMemory::FArchiveMemory(const TArray<uint8>& InBytes)
    : FArchive(EArchiveMode::Loading)
    , WriteBytes(nullptr)
    , ReadBytes(&InBytes)
    , ReadOffset(0)
{
}


#define IMPLEMENT_MEMORY_SERIALIZE(Type) \
    void FArchiveMemory::Serialize(std::string_view Name, Type& Value) \
    { \
        if (IsSaving()) \
        { \
            size_t Size = sizeof(Type); \
            size_t CurrentSize = WriteBytes->size(); \
            WriteBytes->resize(CurrentSize + Size); \
            std::memcpy(WriteBytes->data() + CurrentSize, &Value, Size); \
        } \
        else if (IsLoading()) \
        { \
            size_t Size = sizeof(Type); \
            if (ReadOffset + Size <= ReadBytes->size()) \
            { \
                std::memcpy(&Value, ReadBytes->data() + ReadOffset, Size); \
                ReadOffset += Size; \
            } \
        } \
    }

// Primitives
IMPLEMENT_MEMORY_SERIALIZE(bool)
IMPLEMENT_MEMORY_SERIALIZE(uint8)
IMPLEMENT_MEMORY_SERIALIZE(int32)
IMPLEMENT_MEMORY_SERIALIZE(uint32)
IMPLEMENT_MEMORY_SERIALIZE(int64)
IMPLEMENT_MEMORY_SERIALIZE(uint64)
IMPLEMENT_MEMORY_SERIALIZE(float32)
IMPLEMENT_MEMORY_SERIALIZE(float64)

// Math Types & Guid
IMPLEMENT_MEMORY_SERIALIZE(FGuid)
IMPLEMENT_MEMORY_SERIALIZE(FVector2D)
IMPLEMENT_MEMORY_SERIALIZE(FVector3)
IMPLEMENT_MEMORY_SERIALIZE(FVector4)
IMPLEMENT_MEMORY_SERIALIZE(FQuat)
IMPLEMENT_MEMORY_SERIALIZE(FMatrix)

#undef IMPLEMENT_MEMORY_SERIALIZE

// Engine Core
void FArchiveMemory::Serialize(std::string_view Name, FString& Value)
{
    if (IsSaving())
    {
        uint32 StringLen = static_cast<uint32>(Value.size());
        Serialize(Name, StringLen);

        if (StringLen > 0)
        {
            size_t CurrentSize = WriteBytes->size();
            WriteBytes->resize(CurrentSize + StringLen);
            std::memcpy(WriteBytes->data() + CurrentSize, Value.data(), StringLen);
        }
    }
    else if (IsLoading())
    {
        uint32 StringLen = 0;
        Serialize(Name, StringLen);

        if (StringLen > 0 && (ReadOffset + StringLen) <= ReadBytes->size())
        {
            Value.resize(StringLen);
            std::memcpy(Value.data(), ReadBytes->data() + ReadOffset, StringLen);
            ReadOffset += StringLen;
        }
    }
}

// Scope
void FArchiveMemory::BeginObjectScope(std::string_view Name)
{
    // 바이너리 아카이브에서는 Object Scope 단위 구분을 하지 않음
}
void FArchiveMemory::EndObjectScope()
{
    // 바이너리 아카이브에서는 Object Scope 단위 구분을 하지 않음
}

void FArchiveMemory::BeginArrayScope(std::string_view Name, size_t& ArraySize)
{
    // 배열은 크기정보를 저장해야 함
    uint32 Size32 = static_cast<uint32>(ArraySize);

    // uint32 재사용
    Serialize(Name, Size32);

    if (IsLoading())
    {
        ArraySize = Size32;
    }
}

void FArchiveMemory::EndArrayScope()
{
}