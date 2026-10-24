#pragma once

#include <functional>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <utility>


// Member function pointer rapper
template <typename Class, typename RetType, typename... ArgTypes>
struct TMemFunPtrType
{
	typedef RetType(Class::* Type)(ArgTypes...);
};

// 순수가상클래스
template<typename RetType, typename... ArgTypes>
struct IBaseDelegateInstance
{
	virtual ~IBaseDelegateInstance() = default;

	virtual RetType Execute(ArgTypes...) const = 0;
};

// 전역함수, 람다함수
template<typename FunctorType, typename RetType, typename... ArgTypes>
struct TLambdaDelegateInstance : public IBaseDelegateInstance<RetType, ArgTypes...>
{
public:
	using FFunctor = typename TFunPtrType<RetType, ArgTypes...>::Type;

	explicit TLambdaDelegateInstance(FunctorType InFunctor) : Functor(std::move(InFunctor)) {}

	RetType Execute(ArgTypes... Args) const final
	{
		return Functor(Args...);
	}

private:
	// 전역함수, 람다함수 타입
	mutable FunctorType Functor;
};

// 멤버 함수-> 객체포인터, 멤버함수 포인터
template<class UserClass, typename RetType, typename... ArgTypes>
struct TRawMethodDelegateInstance : public IBaseDelegateInstance<RetType, ArgTypes...>
{
public:
	using FMethodPtr = typename TMemFunPtrType<UserClass, RetType, ArgTypes...>::Type;

	explicit TRawMethodDelegateInstance(UserClass* InUserObject, FMethodPtr InMethodPtr)
		: UserObject(InUserObject), MethodPtr(InMethodPtr) {}

	RetType Execute(ArgTypes... Args) const final
	{
		return (UserObject->*MethodPtr)(Args...);
	}

private:
	UserClass* UserObject{ nullptr };

	// 멤버 함수 포인터
	FMethodPtr MethodPtr{ nullptr };
};




// 델리게이트 등록 해제 식별자 핸들
struct FDelegateHandle
{
	uint64_t ID = 0;

	bool IsValid() const { return ID != 0; }
	void Reset() { ID = 0; }

	bool operator==(const FDelegateHandle& Other) const { return ID == Other.ID; }
};

template<typename... Args>
class TMulticastDelegate
{
public:
	using FCallback = std::function<void(Args...)>;

	// 콜백 함수 등록
	FDelegateHandle Add(FCallback InFunc)
	{
		if (!InFunc) return FDelegateHandle{};

		uint64_t NewID = ++NextID;
		Entries.push_back({ NewID, std::move(InFunc) });
		return FDelegateHandle{ NewID };
	}

	// 멤버 함수 등록
	template<typename UserClass>
	FDelegateHandle AddRaw(UserClass* InUserObject, void(UserClass::* InMethod)(Args...))
	{
		return Add([InUserObject, InMethod](Args... InArgs)
			{
				(InUserObject->*InMethod)(std::forward<Args>(InArgs)...);
			});
	}

	// 특정 핸들러 해제
	bool Remove(FDelegateHandle& InHandle)
	{
		if (!InHandle.IsValid()) return false;

		auto It = std::remove_if(Entries.begin(), Entries.end(),
			[&InHandle](const FEntry& Entry) { return Entry.ID == InHandle.ID; });

		if (It != Entries.end())
		{
			Entries.erase(It, Entries.end());
			InHandle.Reset();
			return true;
		}
		return false;
	}

	// 전체 리스너 일괄 호출
	void Broadcast(Args... InArgs)
	{
		// 복사본 기반 순회
		auto LocalEntries = Entries;
		for (const auto& Entry : LocalEntries)
		{
			if (Entry.Callback)
			{
				Entry.Callback(InArgs...);
			}
		}
	}

	// 전체 해제
	void Clear()
	{
		Entries.clear();
	}

	bool IsBound() const
	{
		return !Entries.empty();
	}

private:
	struct FEntry
	{
		uint64_t ID;
		FCallback Callback;
	};

	std::vector<FEntry> Entries;
	uint64_t NextID = 0;
};
