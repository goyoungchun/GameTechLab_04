#pragma once

#include "GameFramework/Pawn.h"

class UBoxComponent;
class UStaticMeshComponent;
class UVehicleMovementComponent;
class UScriptComponent;
class UCameraComponent;
struct FVehicleInput;

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
	virtual void Tick(float DeltaTime) override;
	void SetVehicleInput(const FVehicleInput& Input);
	void RequestJump();

	uint32 GetPlayerIndex() const { return PlayerIndex; }
	UVehicleMovementComponent* GetMovement() const { return Movement; }

	bool SetBody(const FString& MeshName);
	FString GetBodyName() const;

private:
	UBoxComponent* Box = nullptr;
	UStaticMeshComponent* StaticMesh = nullptr;
	UVehicleMovementComponent* Movement = nullptr;
	UScriptComponent* Script = nullptr;
	UCameraComponent* Camera = nullptr;

	uint32 PlayerIndex = 0;

	float BaseFOV = 60.0f;
	float BoostFOVOffset = 15.0f;
	float FOVInterpSpeed = 6.0f;
};