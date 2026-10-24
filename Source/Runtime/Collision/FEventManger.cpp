#include "EnginePCH.h"

#include "FEventManger.h"
#include "FPhysSceneChaos.h"
#include "Component/PrimitiveComponent.h"
#include "GameFramework/Actor.h"

namespace
{
	ECollisionResponse GetCurrentResponse(const UPrimitiveComponent* A, const UPrimitiveComponent* B)
	{
		if (!A || !B || !A->IsRegistered() || !B->IsRegistered()
			|| (A->GetOwner() && !A->GetOwner()->GetActorEnableCollision())
			|| (B->GetOwner() && !B->GetOwner()->GetActorEnableCollision()))
			return ECollisionResponse::Ignore;
		return ResolveCollisionResponse(A->GetCollisionResponseToChannel(B->GetObjectChannel()),
			B->GetCollisionResponseToChannel(A->GetObjectChannel()));
	}

	FHitResult MakeHitResult(UPrimitiveComponent* Other, bool bForA,
		const FPhysicsCollisionResult& Result, bool bBlocking)
	{
		FHitResult Hit;
		Hit.HitComponent = Other;
		Hit.Distance = 0.0f;
		Hit.ImpactPoint = Result.ContactPoint;
		// Evolution의 A -> B 법선을 받는 컴포넌트가 밀려나는 방향으로 바꾼다.
		Hit.Normal = bForA ? -Result.Normal : Result.Normal;
		Hit.ImpactNormal = Hit.Normal;
		Hit.PenetrationDepth = std::max(0.0f, Result.PenetrationDepth);
		Hit.bBlockingHit = bBlocking;
		Hit.bStartPenetrating = Hit.PenetrationDepth > 0.0f;
		return Hit;
	}
}

FEventManger::FEventManger() = default;
FEventManger::~FEventManger() = default;

void FEventManger::BufferCollisionResults(const TArray<FPhysicsCollisionResult>& InResults)
{
	PendingResults = InResults;
	bHasBufferedResults = true;
}

void FEventManger::DispatchEvents(const FPhysSceneChaos& Scene)
{
	if (!bHasBufferedResults || bDispatching)
		return;
	bHasBufferedResults = false;
	bDispatching = true;

	// 콜백 중 다음 결과가 들어와도 이번 Step의 목록은 변하지 않는다.
	const TArray<FPhysicsCollisionResult> FrameResults = std::move(PendingResults);
	std::map<FBodyPair, FPhysicsCollisionResult> CurrentCollisions;
	for (FPhysicsCollisionResult Result : FrameResults)
	{
		if (Result.BodyIdA == 0 || Result.BodyIdB == 0 || Result.BodyIdA == Result.BodyIdB
			|| Result.Response == ECollisionResponse::Ignore)
			continue;
		if (Result.BodyIdA > Result.BodyIdB)
		{
			std::swap(Result.BodyIdA, Result.BodyIdB);
			Result.Normal = -Result.Normal;
		}

		// DuringPhysics에서 바뀐 채널 설정과 제거 여부도 통지 직전에 확인한다.
		Result.Response = GetCurrentResponse(Scene.FindPrimitive(Result.BodyIdA), Scene.FindPrimitive(Result.BodyIdB));
		if (Result.Response != ECollisionResponse::Ignore)
			CurrentCollisions.emplace(FBodyPair{ Result.BodyIdA, Result.BodyIdB }, Result);
	}

	std::map<FBodyPair, FOverlapState> EndedOverlaps;
	for (auto It = ActiveOverlaps.begin(); It != ActiveOverlaps.end();)
	{
		const auto Current = CurrentCollisions.find(It->first);
		if (Current == CurrentCollisions.end() || Current->second.Response != ECollisionResponse::Overlap)
		{
			EndedOverlaps.emplace(It->first, It->second);
			It = ActiveOverlaps.erase(It);
		}
		else
			++It;
	}
	// Overlap -> Block 전환에서는 EndOverlap이 먼저, Hit가 다음이다.
	for (const auto& [Pair, State] : EndedOverlaps)
		NotifyEndOverlap(Pair, State, Scene);

	for (const auto& [Pair, Result] : CurrentCollisions)
	{
		if (Result.Response == ECollisionResponse::Overlap)
		{
			ActiveOverlaps.try_emplace(Pair);
			NotifyBeginOverlap(Pair, true, Result, Scene);
			NotifyBeginOverlap(Pair, false, Result, Scene);
		}
		else if (Result.Response == ECollisionResponse::Block)
		{
			NotifyHit(Pair, true, Result, Scene);
			NotifyHit(Pair, false, Result, Scene);
		}
	}
	// 앞선 콜백에서 제거되거나 Ignore/Block으로 바뀐 겹침을 같은 Step에 정리한다.
	EndInvalidOverlaps(Scene);
	bDispatching = false;
}

void FEventManger::NotifyBeginOverlap(const FBodyPair& Pair, bool bForA,
	const FPhysicsCollisionResult& Result, const FPhysSceneChaos& Scene)
{
	UPrimitiveComponent* A = Scene.FindPrimitive(Pair.first);
	UPrimitiveComponent* B = Scene.FindPrimitive(Pair.second);
	if (GetCurrentResponse(A, B) != ECollisionResponse::Overlap)
		return;

	FOverlapState& State = ActiveOverlaps.at(Pair);
	bool& bNotified = bForA ? State.bNotifiedA : State.bNotifiedB;
	if (bNotified)
		return;
	bNotified = true;

	UPrimitiveComponent* Self = bForA ? A : B;
	UPrimitiveComponent* Other = bForA ? B : A;
	const FHitResult Hit = MakeHitResult(Other, bForA, Result, false);
	// 콜백에서 컴포넌트를 제거해도 델리게이트 자체를 계속 참조하지 않는다.
	auto Delegate = Self->OnComponentBeginOverlap;
	// 현재 Evolution은 Sweep이 아닌 이산 접촉 검사이며 컴포넌트당 Body는 하나다.
	Delegate.Broadcast(Self, Other->GetOwner(), Other, 0, false, Hit);
}

void FEventManger::NotifyEndOverlap(const FBodyPair& Pair, const FOverlapState& State, const FPhysSceneChaos& Scene)
{
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const bool bForA = Side == 0;
		if (!(bForA ? State.bNotifiedA : State.bNotifiedB))
			continue;
		// 첫 번째 콜백에서 상대를 제거할 수 있으므로 양쪽을 매번 다시 찾는다.
		UPrimitiveComponent* Self = Scene.FindPrimitive(bForA ? Pair.first : Pair.second);
		UPrimitiveComponent* Other = Scene.FindPrimitive(bForA ? Pair.second : Pair.first);
		if (!Self)
			continue;
		auto Delegate = Self->OnComponentEndOverlap;
		// 이전 Step 이후 상대가 제거되었다면 OtherActor/OtherComp는 nullptr이다.
		Delegate.Broadcast(Self, Other ? Other->GetOwner() : nullptr, Other, 0);
	}
}

void FEventManger::NotifyHit(const FBodyPair& Pair, bool bForA,
	const FPhysicsCollisionResult& Result, const FPhysSceneChaos& Scene)
{
	UPrimitiveComponent* A = Scene.FindPrimitive(Pair.first);
	UPrimitiveComponent* B = Scene.FindPrimitive(Pair.second);
	if (GetCurrentResponse(A, B) != ECollisionResponse::Block)
		return;
	UPrimitiveComponent* Self = bForA ? A : B;
	UPrimitiveComponent* Other = bForA ? B : A;
	const FHitResult Hit = MakeHitResult(Other, bForA, Result, true);
	auto Delegate = Self->OnComponentHit;
	// ResolveContacts가 충격량을 계산하기 전까지 NormalImpulse는 0이다.
	Delegate.Broadcast(Self, Other->GetOwner(), Other, FVector::ZeroVector, Hit);
}

void FEventManger::EndInvalidOverlaps(const FPhysSceneChaos& Scene)
{
	std::map<FBodyPair, FOverlapState> EndedOverlaps;
	for (auto It = ActiveOverlaps.begin(); It != ActiveOverlaps.end();)
	{
		if (GetCurrentResponse(Scene.FindPrimitive(It->first.first), Scene.FindPrimitive(It->first.second))
			!= ECollisionResponse::Overlap)
		{
			EndedOverlaps.emplace(It->first, It->second);
			It = ActiveOverlaps.erase(It);
		}
		else
			++It;
	}
	for (const auto& [Pair, State] : EndedOverlaps)
		NotifyEndOverlap(Pair, State, Scene);
}
