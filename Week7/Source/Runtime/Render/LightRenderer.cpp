#include "EnginePCH.h"
#include "LightRenderer.h"
#include "Math/Vector.h"
#include "Math/Matrix.h"
#include "RenderResourceManager.h"
#include "RenderCommand.h"

bool FLightRenderer::Init()
{
	LightShader = FRenderResourceManager::GetShaderProgram("Resources/Shader/FxaaShader.hlsl");
	if (!LightShader)
	{
		HTR_LOG(Error, "[FXAA] shader not found");
		return false;
	}

	//ConstantBuffer = RenderCommand::CreateConstantBuffer(sizeof(FFXAAConstants));

	PipelineState.Shader = LightShader;
	PipelineState.Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	// 결과로 덮어쓰는 후처리라 블렌딩,깊이 테스트가 필요 없음
	PipelineState.BlendState = EBlendState::Opaque;
	PipelineState.DepthStencilState = EDepthStencilState::Disabled;
	return true;
}
