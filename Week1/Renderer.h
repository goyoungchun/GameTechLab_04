#pragma once

#pragma comment(lib, "user32")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")

#include <d3d11.h>
#include <d3dcompiler.h>

#include "Math.h"

struct FVertexSimple
{
	float x, y, z;
	float r, g, b, a;
};

struct FConstants
{
	FVector Offset;
	float Scale;
	FColor Color;
};

class URenderer
{
public:
	// 렌더러 초기화 함수
	void Create(HWND hWindow);
	void CreateDeviceAndSwapChain(HWND hWindow);

	// Direct3D 장치 및 스왑체인을 해제하는 함수
	void ReleaseDeviceAndSwapChain();

	// 프레임 버퍼를 생성하는 함수
	void CreateFrameBuffer();

	// 프레임 버퍼를 해제하는 함수
	void ReleaseFrameBuffer();

	// 레스터라이저 상태를 생성하는 함수
	void CreateRasterizerState();

	// 레스터라이저 상태를 해제하는 함수
	void ReleaseRasterizerState();

	// 랜더러에 사용된 모든 리소스를 해제하는 함수
	void Release();

	// 스왑체인의 백버퍼와 프론트 버퍼를 교체하며 화면에 출력
	void SwapBuffer();

	void CreateShader();
	void ReleaseShader();

	void Prepare();
	void PrepareShader();
	void RenderPrimitive(ID3D11Buffer* pBuffer, UINT numVertices);

	ID3D11Buffer* CreateVertexBuffer(FVertexSimple* vertices, UINT byteWidth);
	void ReleaseVertexBuffer(ID3D11Buffer* vertexBuffer);

	void CreateConstantBuffer();
	void ReleaseConstantBuffer();
	void UpdateConstant(FVector offset, float scale, FColor color);

public:
	// Direct3D 11 장치와 장치 컨텍스트 및 스왑 체인 관리를 위한 포인터들
	ID3D11Device* Device = nullptr; // GPU와 통신하기 위한 Direct3d 장치
	ID3D11DeviceContext* DeviceContext = nullptr; // GPU 명령 실행을 담당하는 컨텍스트
	IDXGISwapChain* SwapChain = nullptr; // 프레임 버퍼를 교체하는 데 사용되는 스왑체인

	// 렌더링에 필요한 리소스 및 상태를 관리하기 위한 변수들
	ID3D11Texture2D* FrameBuffer = nullptr;
	ID3D11RenderTargetView* FrameBufferRTV = nullptr; // 텍스처를 렌더 타겟으로 사용하는 뷰
	ID3D11RasterizerState* RasterizerState = nullptr; // 래스터라이저 상태 컬링, 채우기 모드 정의
	ID3D11Buffer* ConstantBuffer = nullptr; // 쉐이더에 데이터를 전달하기 위한 상수버퍼

	FLOAT ClearColor[4] = { 0.025f, 0.025f , 0.025f , 1.0f };
	D3D11_VIEWPORT ViewportInfo; // 렌더링 영역을 정의하는 뷰포트 정보

	ID3D11VertexShader* SimpleVertexShader;
	ID3D11PixelShader* SimplePixelShader;
	ID3D11InputLayout* SimpleInputLayout;
	unsigned int Stride;


	// Todo: Fix
	ID3D11Buffer* VertexBufferSphere;
	UINT NumVerticesSphere;
	ID3D11Buffer* VertexBufferTriangle;
	UINT NumVerticesTriangle;
	ID3D11Buffer* VertexBufferSquare;
	UINT NumVerticesSquare;
};