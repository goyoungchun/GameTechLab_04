#pragma once

#include "Engine/Scene/UScene.h"
#include "Engine/Editor/Controller/FCameraController.h"
#include "../Editor/ObjectPicker/FObjectPicker.h"

////////////////
// UMainScene //
////////////////
class UMainScene : public UScene
{
    UCLASS(UMainScene, "MainScene", UScene)

public:
	virtual void BeginPlay() override;

	virtual void Tick(float DeltaTime) override;

	virtual void EndPlay() override;

};
