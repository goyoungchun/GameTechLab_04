#pragma once

#include "Container/FString.h"
#include "Engine/Object/USceneComponent.h"
#include "Engine/Renderer/FPrimitiveRenderData.h"
#include "../../GResourceManager.h"
struct FMeshResource;

class UPrimitiveComponent : public USceneComponent
{

    UCLASS(UPrimitiveComponent, "PrimitiveComponent", USceneComponent)

protected:

    static FPrimitiveRenderData CreateRenderData(FStringView Type);

public:

    virtual void Initialize() override;

    // 렌더러에게 전달할 렌더 정보
    const FPrimitiveRenderData& GetRenderData() const { return RenderData; }

    FMeshResource* GetMeshResource() const
    {
        return GResourceManager::GetInstance()->GetPrimitive(FString(GetInstanceClass()->Name));
    }

    FPrimitiveRenderData RenderData;
};

