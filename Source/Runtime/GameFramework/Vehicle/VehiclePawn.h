#pragma once

#include "GameFramework/Pawn.h"

class UBoxComponent;
class UStaticMeshComponent;
class UStaticMesh;
class UVehicleMovementComponent;
class UScriptComponent;
class UCameraComponent;
struct FVehicleInput;

class AVehiclePawn : public APawn
{
	DECLARE_CLASS(AVehiclePawn, APawn)
	REFLECT_START(ClassName)
	PROPERTY(PlayerIndex)
	PROPERTY_RANGE(CameraDistanceScale, 2.0f, 10.0f)
	PROPERTY_RANGE(MinCameraDistance, 1.0f, 100.0f)
	PROPERTY(CameraOffset)
	PROPERTY(LookAtOffset)
	PROPERTY(RotationFollowSpeed)
	PROPERTY(PositionFollowSpeed)
	PROPERTY(FOVFollowSpeed)
	REFLECT_END()

public:
	AVehiclePawn();
	virtual ~AVehiclePawn() override = default;
	virtual void BeginPlay() override;
	virtual void EndPlay() override;
	virtual void Tick(float DeltaTime) override;

	int32 GetPlayerIndex() const { return static_cast<int32>(PlayerIndex); }
	void SetPlayerIndex(int32 InPlayerIndex) { PlayerIndex = static_cast<int32>(InPlayerIndex); }

	void SetVehicleInput(const FVehicleInput& Input);
	void RequestJump();

	UVehicleMovementComponent* GetMovement() const { return Movement; }
	UStaticMeshComponent* GetBodyMeshComponent() const { return StaticMesh; }

	bool SetBody(const FString& MeshName);
	FString GetBodyName() const;
	void FitBodyCollision();
	void ResetFollowCamera();

private:
	void RefreshVehicleVisuals();
	void ClearVehicleVisuals();
	void UpdateWheelVisuals(float DeltaTime);
	void UpdateFollowCamera(float DeltaTime);
	UBoxComponent* Box = nullptr;
	UStaticMeshComponent* StaticMesh = nullptr;
	UStaticMeshComponent* BodyVisual = nullptr;
	UStaticMeshComponent* Wheels[4] = {};
	float WheelSpin[4] = {};
	float WheelSteering = 0.0f;
	UStaticMesh* AppliedBodyMesh = nullptr;
	bool bUsingParts = false;
	bool bSourceWasVisible = true;
	UVehicleMovementComponent* Movement = nullptr;
	UScriptComponent* Script = nullptr;
	UCameraComponent* Camera = nullptr;

	uint32 PlayerIndex = 0;

	float BaseFOV = 60.0f;
	float BoostFOVOffset = 15.0f;
	FVector CameraOffset = FVector(-20.0f, 0.0f, 8.0f);
	FVector LookAtOffset = FVector(1.5f, 0.0f, 0.8f);
	float RotationFollowSpeed = 6.0f;
	float PositionFollowSpeed = 12.0f;
	float FOVFollowSpeed = 6.0f;
	float FollowYaw = 0.0f;
	FVector FollowPosition = FVector::ZeroVector;
	bool bCameraInitialized = false;
	float CameraDistanceScale = 5.0f;
	float MinCameraDistance = 10.0f;
};
