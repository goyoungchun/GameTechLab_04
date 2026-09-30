#include <windows.h>
#include <iostream>

#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"

#include "Math.h"
#include "Renderer.h"
#include "Sphere.h"
#include "Primitive.h"
#include "Ball.h"
#include "Circle.h"
#include "Triangle.h"
#include "InputManager.h"
#include "Player.h"
#include "Map.h"

enum ETypePrimitive
{
	EPT_Triangle,
	EPT_Cube,
	EPT_Sphere,
	EPT_Max
};

int UBall::TotalNumBalls = 0;
bool UBall::bGravity = true;

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

// 각종 메시지를 처리할 함수
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
	{
		return true;
	}

	switch (message) 
	{
	case WM_KEYDOWN:
		switch (wParam)
		{
		case VK_SPACE:
			InputManager::GetInstance()->OnKeyDown(SPACE);
			break;
		case VK_LEFT:
			InputManager::GetInstance()->OnKeyDown(LEFT);
			break;
		case VK_RIGHT:
			InputManager::GetInstance()->OnKeyDown(RIGHT);
			break;
		case VK_UP:
			InputManager::GetInstance()->OnKeyDown(UP);
			break;
		case VK_DOWN:
			InputManager::GetInstance()->OnKeyDown(DOWN);
			break;
		case VK_RETURN:
			InputManager::GetInstance()->OnKeyDown(ENTER);
			break;
		}
		return 0;
	case WM_KEYUP:
		switch (wParam)
		{
		case VK_SPACE:
			InputManager::GetInstance()->OnKeyUP(SPACE);
			break;
		case VK_LEFT:
			InputManager::GetInstance()->OnKeyUP(LEFT);
			break;
		case VK_RIGHT:
			InputManager::GetInstance()->OnKeyUP(RIGHT);
			break;
		case VK_UP:
			InputManager::GetInstance()->OnKeyUP(UP);
			break;
		case VK_DOWN:
			InputManager::GetInstance()->OnKeyUP(DOWN);
			break;
		case VK_RETURN:
			InputManager::GetInstance()->OnKeyUP(ENTER);
			break;
		}
		return 0;
	case WM_DESTROY:
		// Signal that the app should quit
		OutputDebugStringA("!!! WM_DESTROY !!!\n");
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPervInstance, LPSTR lpCmdLine, int nShowCmd)
{
	// 윈도우 클래스 이름
	WCHAR WindowClass[] = L"jungleWindowClass";

	// 윈도우 클래스 이름
	WCHAR Title[] = L"Game Tech Lab";

	// 각종 메시지를 처리할 함수인 WndProc의 함수 포인터를 WindowClass 구조체에 넣는다.
	WNDCLASSW wndclass = {0, WndProc, 0, 0, 0, 0, 0, 0, 0, WindowClass };

	// 윈도우 클래스 등록
	RegisterClassW(&wndclass);

	// 1024x1024 크기에 윈도우 생성
	HWND hWnd = CreateWindowExW(0, WindowClass, Title, WS_POPUP | WS_VISIBLE | WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, 1024, 1024, nullptr, nullptr, hInstance, nullptr);

	// 각종 생성하는 코드를 여기에 추가

	// Renderer Class 생성
	URenderer renderer;

	// D3D11 생성하는 함수를 호출
	renderer.Create(hWnd);
	renderer.CreateShader();
	renderer.CreateConstantBuffer();

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	ImGui_ImplWin32_Init((void*)hWnd);
	ImGui_ImplDX11_Init(renderer.Device, renderer.DeviceContext);

	// 버텍스 생성
	FVertexSimple triangle_vertices[] =
	{
		{  0.0f,  1.0f, 0.0f,  1.0f, 0.0f, 0.0f, 1.0f }, // Top vertex (red)
		{  1.0f, -1.0f, 0.0f,  0.0f, 1.0f, 0.0f, 1.0f }, // Bottom-right vertex (green)
		{ -1.0f, -1.0f, 0.0f,  0.0f, 0.0f, 1.0f, 1.0f }  // Bottom-left vertex (blue)
	};

	FVertexSimple square_vertices[] =
	{
		// Front face (Z+)
		{ -0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 0.0f, 1.0f }, // Bottom-left (red)
		{ -0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 0.0f, 1.0f }, // Top-left (yellow)
		{  0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 0.0f, 1.0f }, // Bottom-right (green)
		{ -0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 0.0f, 1.0f }, // Top-left (yellow)
		{  0.5f,  0.5f,  0.5f,  0.0f, 0.0f, 1.0f, 1.0f }, // Top-right (blue)
		{  0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 0.0f, 1.0f }, // Bottom-right (green)
	};

	// 버텍스 버퍼 생성
	UINT numVerticesTriangle = sizeof(triangle_vertices) / sizeof(FVertexSimple);
	UINT numVerticesCube = sizeof(square_vertices) / sizeof(FVertexSimple);
	UINT numVerticesSphere = sizeof(sphere_vertices) / sizeof(FVertexSimple);

	ID3D11Buffer* vertexBufferTriangle = renderer.CreateVertexBuffer(triangle_vertices, sizeof(triangle_vertices));
	ID3D11Buffer* vertexBufferSquare = renderer.CreateVertexBuffer(square_vertices, sizeof(square_vertices));
	ID3D11Buffer* vertexBufferSphere = renderer.CreateVertexBuffer(sphere_vertices, sizeof(sphere_vertices));


	// Todo: Fix
	renderer.VertexBufferSphere = renderer.CreateVertexBuffer(sphere_vertices, sizeof(sphere_vertices));
	renderer.NumVerticesSphere = sizeof(sphere_vertices) / sizeof(FVertexSimple);
	renderer.VertexBufferTriangle = renderer.CreateVertexBuffer(triangle_vertices, sizeof(triangle_vertices));
	renderer.NumVerticesTriangle = sizeof(triangle_vertices) / sizeof(FVertexSimple);

	ETypePrimitive typePrimitive = EPT_Cube;

	int count = 0;
	int MaxCount = 50;

	UPrimitive** PrimitiveList;
	PrimitiveList = new UPrimitive* [MaxCount];
	
	for (int i = 0; i < MaxCount; i++)
	{
		PrimitiveList[i] = nullptr;
	}

	// 도형의 움직임 정도를 담을 offset 변수
	FVector offset(0.0f);
	FVector velocity(0.0f);

	const float leftBorder = -1.0f;
	const float rightBorder = 1.0f;
	const float topBorder = -1.0f;
	const float bottomBorder = 1.0f;
	const float sphereRadius= 1.0f;
	const float ballSpeed = 1.0f;

	const int targetFPS = 30;
	const double targetFrameTime = 1000.0 / targetFPS;

	LARGE_INTEGER frequency;
	QueryPerformanceFrequency(&frequency);

	LARGE_INTEGER startTime, endTime;
	double elapsedTime = 0.0;

	float DeltaTime = (float)(elapsedTime / 1000.0);
	bool bIsExit = false;

	/*
	Circle* testCircle;
	testCircle = new Circle();
	testCircle->setPos(FVector{ 0.0f, 0.1f, 0.2f });
	testCircle->setRadius(0.1f);
	testCircle->setColor(FColor{ 0.5f, 0.1f, 0.2f });

	Circle* testCircle2;
	testCircle2 = new Circle();
	testCircle2->setPos(FVector{ 0.5f, 0.1f, 0.2f });
	testCircle2->setRadius(0.1f);
	testCircle2->setColor(FColor{ 0.5f, 0.5f, 0.2f });

	Circle* testCircle3;
	testCircle3 = new Circle();
	testCircle3->setPos(FVector{ 0.0f, 0.5f, 0.2f });
	testCircle3->setRadius(0.1f);
	testCircle3->setColor(FColor{ 0.2f, 0.3f, 0.5f });


	Triangle* testTris = new Triangle(FVector{ 0.0f, 0.7f, 0.2f }, FColor{ 0.5f, 0.3f, 0.5f }, 0.1f);
	delete testTris;

	//Renderable::RenderAll(renderer);

	delete testCircle;
	delete testCircle2;
	delete testCircle3;

	testCircle3 = new Circle();
	testCircle3->setPos(FVector{ 0.0f, 0.5f, 0.2f });
	testCircle3->setRadius(0.1f);
	testCircle3->setColor(FColor{ 0.2f, 0.3f, 0.5f });
	testTris = new Triangle(FVector{ 0.0f, 0.7f, 0.2f }, FColor{ 0.5f, 0.3f, 0.5f }, 0.1f);
	*/

	UPlayer player;


	UMap* testMap = new UMap();
	testMap->randMapGenerator();

	/*testMap->addBallandPop(3, 3, Green);
	testMap->addBallandPop(4, 3, Green);
	testMap->addBallandPop(4, 4, Blue);
	testMap->addBallandPop(4, 5, Blue);
	testMap->addBallandPop(5, 5, Red);
	testMap->addBallandPop(5, 6, Red);
	testMap->addBallandPop(5, 4, Purple);
	testMap->addBallandPop(6, 3, Green);
	testMap->addBallandPop(7, 4, Green);
	testMap->addBallandPop(7, 5, Red);
	testMap->addBallandPop(8, 5, Red);
	testMap->addBallandPop(7, 6, Purple);*/

	//testMap->addBallandPop(5, 4, Lightening);
	testMap->addBallandPop(5, 4, Bomb);
	
	testMap->addBallandPop(0, 4, Bomb);
	testMap->addBallandPop(9, 3, Bomb);

	testMap->renderMap();

	
	player.SetMap(testMap);

	// Main Loop (Quit Message가 들어오기 전까지 아래 Loop를 무한히 실행하게 됨)
	while (bIsExit == false)
	{
		DeltaTime = (float)(elapsedTime / 1000.0);

		QueryPerformanceCounter(&startTime);

		MSG msg;

		// 처리할 메시지가 더 이상 없을 때까지 수행
		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			// 키 입력 메시지를 번역
			TranslateMessage(&msg);

			// 메시지를 적절한 위도우 프리시저에 전달, 메시지가 위에서 등록한 WndProc 으로 전달됨
			DispatchMessage(&msg);

			/*
			if (msg.message == WM_LBUTTONDOWN)
			{
				
				testMap->addBallandPop(5, 5, Green);
				//testMap->addBallandPop(7, 3, Green);

				
				testMap->addLine();
				
				//testMap->renderMap();

				break;
			}

			if (msg.message == WM_CHAR)
			{
				testMap->addLine();
				testMap->renderMap();
				break;
			}
			*/

			if (msg.message == WM_QUIT)
			{
				bIsExit = true;
				break;
			}
		}

		////////////////////////////////////////////////////
		// 매번 실행되는 코드를 여기에 추가
		player.SetMap(testMap);
		player.Update();
		testMap->DropBallUpdate(DeltaTime);


		// 준비 작업
		renderer.Prepare();
		renderer.PrepareShader();

		Renderable::RenderAll(renderer);

		
		/*
		if (count < 0)
		{
			count = 0;
		}
		
		if (count >= MaxCount)
		{
			int NewMaxCount = MaxCount * 2;

			UPrimitive** NewList = new UPrimitive* [NewMaxCount];

			for (int i = 0; i < MaxCount; i++)
			{
				NewList[i] = PrimitiveList[i];
			}

			for (int i = MaxCount; i < NewMaxCount; i++)
			{
				NewList[i] = nullptr;
			}

			delete[] PrimitiveList;

			PrimitiveList = NewList;
			MaxCount = NewMaxCount;
		}

		// Ball 생성
		if (count > UBall::TotalNumBalls)
		{
			int diff = count - UBall::TotalNumBalls;

			for (int i = 0; i < MaxCount; i++)
			{
				if (PrimitiveList[i] == nullptr && diff > 0)
				{
					PrimitiveList[i] = new UBall;
					diff--;
				}
			}
		}

		// Ball 소멸
		if (count < UBall::TotalNumBalls)
		{
			int diff = UBall::TotalNumBalls - count;

			while (diff != 0)
			{
				int curr = UBall::TotalNumBalls;

				int index = rand() % MaxCount;

				if (PrimitiveList[index] == nullptr) continue;

				delete PrimitiveList[index];

				PrimitiveList[index] = nullptr;

				diff--;
			}
		}

		// Update
		for (int i = 0; i < MaxCount; i++)
		{
			UPrimitive* a = PrimitiveList[i];

			if (a == nullptr) continue;

			float scaleMod = a->GetRadius();

			a->Gravity(DeltaTime);

			offset = a->GetLocation();
			velocity = a->GetVelocity();

			offset.x += velocity.x * DeltaTime;
			offset.y += velocity.y * DeltaTime;

			float renderRadius = sphereRadius * scaleMod;
			if (offset.x < leftBorder + renderRadius)
			{
				offset.x = leftBorder + renderRadius;
				velocity.x *= -1.0f;
			}
			if (offset.x > rightBorder - renderRadius)
			{
				offset.x = rightBorder - renderRadius;
				velocity.x *= -1.0f;
			}
			if (offset.y < topBorder + renderRadius)
			{
				offset.y = topBorder + renderRadius;
				velocity.y *= -1.0f;
			}
			if (offset.y > bottomBorder - renderRadius)
			{
				offset.y = bottomBorder - renderRadius;
				velocity.y *= -1.0f;
			}

			a->SetLocation(offset);
			a->SetVelocity(velocity);

			// offset을 상수 버퍼로 업데이트
			FColor tempColor = { 1.0, 0.0, 0.0, 1.0 };
			renderer.UpdateConstant(offset, scaleMod, tempColor);
			//renderer.RenderPrimitive(vertexBufferSphere, numVerticesSphere);

			// Todo: Fix
			renderer.RenderPrimitive(renderer.VertexBufferSphere, renderer.NumVerticesSphere);

			switch (typePrimitive)
			{
			case EPT_Triangle:
				renderer.RenderPrimitive(vertexBufferTriangle, numVerticesTriangle);
				break;
			case EPT_Cube:
				renderer.RenderPrimitive(vertexBufferSquare, numVerticesCube);
				break;
			case EPT_Sphere:
				renderer.RenderPrimitive(vertexBufferSphere, numVerticesSphere);
				break;
			}
		}
		
		for (int i = 0; i < MaxCount; i++) 
		{
			UPrimitive* a = PrimitiveList[i];

			if (a == nullptr) continue;

			for (int j = i + 1; j < MaxCount; j++) 
			{
				UPrimitive* b = PrimitiveList[j];

				if (b == nullptr ) continue;

				if (a->CheckCollision(b))
				{
					a->ResolveCollision(b);
				}
			}
		}
		*/
		/*
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		// 이후 ImGui UI 컨트롤 추가는 여기에 위치
		ImGui::Begin("Jungle Property Window");
		{
			ImGui::Text("Hello Jungle World!");
			ImGui::Checkbox("Gravity", &UBall::bGravity);

			ImGui::InputInt("Number of Balls", &count);
		}
		ImGui::End();

		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
		*/

		// 현재 화면에 보여지는 버퍼와 그리기 작업을 위한 버퍼를 서로 교환
		renderer.SwapBuffer();

		do
		{
			Sleep(0);

			QueryPerformanceCounter(&endTime);

			elapsedTime = (endTime.QuadPart - startTime.QuadPart) * 1000.0 / frequency.QuadPart;

		} while (elapsedTime < targetFrameTime);
		////////////////////////////////////////////////////
	}
	
	// 소멸하는 코드를 여기에 추가
	
	/*
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	*/

	renderer.ReleaseVertexBuffer(vertexBufferTriangle);
	renderer.ReleaseVertexBuffer(vertexBufferSquare);
	renderer.ReleaseVertexBuffer(vertexBufferSphere);

	renderer.ReleaseConstantBuffer();
	renderer.ReleaseShader();
	renderer.Release();

	return 0;
}