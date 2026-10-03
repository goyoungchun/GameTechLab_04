#pragma once

#include <cassert>
#include <unordered_set>
#include "Core.h"
#include "TArray.h"

template <typename T>
class TSet
{
public:
	TSet() = default;
	~TSet() = default;

	bool Add(const T& data);

	// Returns removed count
	int32 Remove(const T& data);

	uint32 Num() const;
	void Reset();

	bool Contains(const T& data) const;
	bool IsEmpty() const;
	void Reserve(int32 capacity);

	std::unordered_set<T>::iterator begin() { return mSet.begin(); }
	std::unordered_set<T>::iterator end() { return mSet.end(); }

	std::unordered_set<T>::const_iterator begin() const { return mSet.begin(); }
	std::unordered_set<T>::const_iterator end() const { return mSet.end(); }

	T& Last()
	{
		if (mSet.empty())
		{
			throw std::runtime_error("TSet::Last() called on empty set");
		}

		return const_cast<T&>(*std::prev(mSet.end()));
	}

	TArray<T> ToArray() const
	{
		TArray<T> result;
		result.Reserve(static_cast<int32>(mSet.size()));
		for (const T& element : mSet)
		{
			result.Add(element);
		}
		return result;
	}

	/*
	void Empty(int32 ExpectedNumElements = 0)
	*/

private:
	std::unordered_set<T> mSet;
};

template<typename T>
inline bool TSet<T>::Add(const T& data)
{
	return mSet.insert(data).second;
}

template<typename T>
inline int32 TSet<T>::Remove(const T& data)
{
	return mSet.erase(data);
}

template<typename T>
inline uint32 TSet<T>::Num() const
{
	return static_cast<uint32>(mSet.size());
}

template<typename T>
inline void TSet<T>::Reset()
{
	mSet.clear();
}

template<typename T>
inline bool TSet<T>::Contains(const T& data) const
{
	return mSet.find(data) != mSet.end() ? true : false;
}

template<typename T>
inline bool TSet<T>::IsEmpty() const
{
	return mSet.size() == 0 ? true : false;
}

template<typename T>
inline void TSet<T>::Reserve(int32 capacity)
{
	mSet.reserve(capacity);
}

