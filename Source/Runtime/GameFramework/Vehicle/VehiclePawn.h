#pragma once

#include "GameFramework/Pawn.h"

class UBoxComponent;
class UStaticMeshComponent;
class UVehicleMovementComponent;
class UScriptComponent;
class UCameraComponent;

class AVehiclePawn : public APawn
{
	DECLARE_CLASS(AVehiclePawn, APawn)
	REFLECT_START(ClassName)
	PROPERTY(PlayerIndex)
	REFLECT_END()

public:
	AVehiclePawn();
	virtual ~AVehiclePawn() override = default;
	virtual void BeginPlay() override;

	int32 GetPlayerIndex() const { return static_cast<int32>(PlayerIndex); }
	void SetPlayerIndex(int32 InPlayerIndex) { PlayerIndex = static_cast<int32>(InPlayerIndex); }

private:
	UBoxComponent* Box = nullptr;
	UStaticMeshComponent* StaticMesh = nullptr;
	UVehicleMovementComponent* Movement = nullptr;
	UScriptComponent* Script = nullptr;
	UCameraComponent* Camera = nullptr;

	uint32 PlayerIndex = 0;
};