#pragma once

#include "FArchive.h"

class FArchiveMemory : public FArchive
{
public:
    // Save 
    FArchiveMemory(TArray<uint8>& InBytes);
    // Load 
    FArchiveMemory(const TArray<uint8>& InBytes);


    // Primitives
    virtual void Serialize(std::string_view Name, bool& Value) override;
    virtual void Serialize(std::string_view Name, uint8& Value) override;
    virtual void Serialize(std::string_view Name, int32& Value) override;
    virtual void Serialize(std::string_view Name, uint32& Value) override;
    virtual void Serialize(std::string_view Name, int64& Value) override;
    virtual void Serialize(std::string_view Name, uint64& Value) override;
    virtual void Serialize(std::string_view Name, float32& Value) override;
    virtual void Serialize(std::string_view Name, float64& Value) override;

    // Engine Core
    virtual void Serialize(std::string_view Name, FString& Value) override;
    virtual void Serialize(std::string_view Name, FGuid& Value) override;

    // Math Type
    virtual void Serialize(std::string_view Name, FVector2D& Value) override;
    virtual void Serialize(std::string_view Name, FVector3& Value) override;
    virtual void Serialize(std::string_view Name, FVector4& Value) override;
    virtual void Serialize(std::string_view Name, FQuat& Value) override;
    virtual void Serialize(std::string_view Name, FMatrix& Value) override;

    // Scope
    virtual void BeginObjectScope(std::string_view Name) override;
    virtual void EndObjectScope() override;
    virtual void BeginArrayScope(std::string_view Name, size_t& ArraySize) override;
    virtual void EndArrayScope() override;

private:
    // Save 
    TArray<uint8>* WriteBytes;

    // Load 
    const TArray<uint8>* ReadBytes;
    size_t ReadOffset;
};