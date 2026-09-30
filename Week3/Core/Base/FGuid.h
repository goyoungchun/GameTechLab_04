#pragma once

struct FGuid {
    uint32 A = 0;
    uint32 B = 0;
    uint32 C = 0;
    uint32 D = 0;

    static FGuid NewGuid();
    FString ToString() const;
    bool Parse(const FString& GuidString);

    inline bool IsValid() const {
        return (A | B | C | D) != 0;
    }

    inline size_t GetHash() const noexcept {
        size_t Hash = std::hash<uint32>{}(A);
        Hash ^= std::hash<uint32>{}(B)+0x9e3779b9 + (Hash << 6) + (Hash >> 2);
        Hash ^= std::hash<uint32>{}(C)+0x9e3779b9 + (Hash << 6) + (Hash >> 2);
        Hash ^= std::hash<uint32>{}(D)+0x9e3779b9 + (Hash << 6) + (Hash >> 2);
        return Hash;
    }

    inline bool operator==(const FGuid& Other) const {
        return (A == Other.A) && (B == Other.B) && (C == Other.C) && (D == Other.D);
    }

    inline bool operator!=(const FGuid& Other) const {
        return !(*this == Other);
    }
};

namespace std {
    template<>
    struct hash<FGuid> {
        size_t operator()(const FGuid& Guid) const noexcept {
            return Guid.GetHash();
        }
    };
}