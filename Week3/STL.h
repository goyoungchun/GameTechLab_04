#pragma once

#include "Core/Memory/TEngineAllocator.h"

// 1. C/C++ 표준 수학 라이브러리 
#include <cmath>
#include <cstdlib>

// STL 컨테이너 및 메모리
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <set>
#include <functional>
#include <array>
#include <algorithm>
#include <optional>

// ----------------------------------------------------
// [ 언리얼 엔진(UE) 스타일 Aliasing ]
// ----------------------------------------------------

// 문자열
using FString = std::basic_string<char, std::char_traits<char>, TEngineAllocator<char, Memory::EMemoryTag::String>>;

// 동적 배열
template <typename T>
using TArray = std::vector<T, TEngineAllocator<T, Memory::EMemoryTag::Container>>;

// 맵/셋
template <typename Key, typename Value>
using TMap = std::unordered_map<Key, Value, std::hash<Key>, std::equal_to<Key>,
                                TEngineAllocator<std::pair<const Key, Value>, Memory::EMemoryTag::Container>>;
template <typename T>
using TSet = std::unordered_set<T, std::hash<T>, std::equal_to<T>,
                                TEngineAllocator<T, Memory::EMemoryTag::Container>>;

// 스마트 포인터 
template <typename T> using TUniquePtr = std::unique_ptr<T>;
template <typename T> using TSharedPtr = std::shared_ptr<T>;
template <typename T> using TWeakPtr = std::weak_ptr<T>;

// 힙 할당이 없으므로 Allocator 불필요
template <typename T, size_t N> using TFixedArray = std::array<T, N>;
template <typename T, typename K> using TPair = std::pair<T, K>;
template <typename... Types> using TTuple = std::tuple <Types...>;