#pragma once

#include "Object.h"

class FObjectIterator
{
public:
	FObjectIterator()
		: mIterator(UObject::GetGObjectArray().begin())
		, mEndIterator(UObject::GetGObjectArray().end())
	{
	}

	UObject* operator*() const
	{
		return *mIterator;
	}

	FObjectIterator& operator++()
	{
		++mIterator;
		return *this;
	}

	explicit operator bool() const
	{
		return mIterator != mEndIterator;
	}

private:
	TSparseArray<UObject*>::Iterator mIterator;
	TSparseArray<UObject*>::Iterator mEndIterator;
};

template <typename T>
class TObjectIterator
{
public:
	TObjectIterator()
	{
		skipToValid();
	}

	T* operator*() const
	{
		return static_cast<T*>(*mIterator);
	}

	TObjectIterator& operator++()
	{
		++mIterator;
		skipToValid();
		return *this;
	}

	explicit operator bool() const
	{
		return static_cast<bool>(mIterator);
	}

private:
	void skipToValid()
	{
		while (mIterator)
		{
			UObject* object = *mIterator;

			if (object != nullptr && object->IsA<T>())
			{
				break;
			}

			++mIterator;
		}
	}

private:
	FObjectIterator mIterator;
};
