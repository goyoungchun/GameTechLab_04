#pragma once

#include <cstdint>
#include <limits>

struct FObjectHandle {
	static constexpr std::uint32_t InvalidIndex = std::numeric_limits<std::uint32_t>::max();

	std::uint32_t Index = InvalidIndex;
	std::uint32_t Generation = 0;

	bool IsValid() const {
		return Index != InvalidIndex;
	}

	bool operator==(const FObjectHandle&) const = default;
};
