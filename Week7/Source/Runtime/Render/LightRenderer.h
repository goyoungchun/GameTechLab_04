#pragma once
#include "Render/PipelineState.h"
#include "Render/Buffer.h"


class FMatrix;
class FVector;


class FTexture2D;

class FLightRenderer final 
{
public:
	FLightRenderer() = default;
	~FLightRenderer() = default;

	bool Init();
	void OnRender(FTexture2D* InputTexture, const FMatrix& ViewProjection, const FVector& CameraPosition);

	FLightRenderer(const FLightRenderer&) = delete;
	FLightRenderer& operator=(const FLightRenderer&) = delete;


private:
	FShaderProgram* LightShader = nullptr;
	FPipelineState PipelineState;
	TUniquePtr<FConstantBuffer> ConstantBuffer;



};