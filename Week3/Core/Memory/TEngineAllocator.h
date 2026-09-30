#pragma once

#include "Memory.h"

#include <cstddef>
#include <type_traits>
#include <limits>
#include <new>

template <typename T, Memory::EMemoryTag Tag = Memory::EMemoryTag::Container>
class TEngineAllocator
{
public:
	using value_type = T;
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;

	using is_always_equal = std::true_type;

	constexpr TEngineAllocator() noexcept = default;

	template <typename U>
	constexpr TEngineAllocator(const TEngineAllocator<U, Tag>&) noexcept {}

	T* allocate(size_type Count)
	{
		if (Count > max_size())
		{
			throw std::bad_array_new_length();
		}

		return static_cast<T*>(Memory::Allocate(Count * sizeof(T), alignof(T), Tag));
	}

	void deallocate(T* Ptr, size_type) noexcept
	{
		Memory::Free(Ptr);
	}

	constexpr size_type max_size() const noexcept
	{
		return (std::numeric_limits<size_type>::max)() / sizeof(T);
	}

	template <typename U>
	struct rebind
	{
		using other = TEngineAllocator<U, Tag>;
	};

};

template<typename T, typename U, Memory::EMemoryTag Tag>
constexpr bool operator==(const TEngineAllocator<T, Tag>&, const TEngineAllocator<U, Tag>&) noexcept
{
	return true;
}

template<typename T, typename U, Memory::EMemoryTag Tag>
constexpr bool operator!=(const TEngineAllocator<T, Tag>&, const TEngineAllocator<U, Tag>&) noexcept
{
	return false;
}
