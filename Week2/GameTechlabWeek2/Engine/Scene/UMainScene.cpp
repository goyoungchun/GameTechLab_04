#include "UMainScene.h"
#include "Engine/GEngine.h"
#include "Engine/Core.h"
#include "Engine/InputManager/GInputManager.h"
#include "Engine/Object/Primitive/USphereComponent.h"
#include "Engine/Object/Primitive/UCubeComponent.h"
#include "Engine/Object/Primitive/UPepeComponent.h"
#include "Engine/Object/Primitive/UOctopusComponent.h"
#include "Engine/Object/Primitive/UArrowRedComponent.h"
#include "Engine/Object/Primitive/UArrowGreenComponent.h"
#include "Engine/Object/Primitive/UArrowBlueComponent.h"
#include "Engine/Object/UCameraComponent.h"
// TEMP(UI test): Gizmo implementation is currently excluded from the build.
// #include "Engine/Gizmo/UWorldAxisGizmo.h"

#include "Engine/Object/Primitive/USphereComponent.h"
#include "Engine/Object/Primitive/UCubeComponent.h"
#include "Engine/Object/Primitive/UPlaneComponent.h"

#include <format>

//////////////////
//  UMainScene  //
//////////////////
void UMainScene::BeginPlay()
{
    //UScene::BeginPlay();

    //CreateMainCamera();
}

void UMainScene::Tick(float DeltaTime)
{
    //UScene::Tick(DeltaTime);

}

void UMainScene::EndPlay()
{
	//UScene::EndPlay();
}
