# Local Shadow Atlas

Spot Light와 Point Light의 그림자를 **하나의 깊이 텍스처** 안에 어떻게 저장하고, 자리를 나눠 주고, 그리고, 셰이더에서 읽는지 설명합니다. 각 단계의 수식도 함께 다룹니다.

**범위.** Spot / Point Light만 다룹니다. Directional Light(CSM)는 여전히 `Texture2DArray`(`t13`)를 사용하며 이 문서의 범위가 아닙니다.

**관련 문서.** 타일 번호를 매기는 방식(Z-order)은 [ShadowAtlasZOrder.md](ShadowAtlasZOrder.md)에서 쉽게 풀어 설명합니다.

---

## 목차

1. [그림자 매핑 한 장 요약](#1-그림자-매핑-한-장-요약)
2. [왜 아틀라스인가](#2-왜-아틀라스인가)
3. [구조 한눈에 보기](#3-구조-한눈에-보기)
4. [한 프레임의 흐름](#4-한-프레임의-흐름)
5. [데이터 모델](#5-데이터-모델)
6. [타일 할당: Z-order 쿼드트리](#6-타일-할당-z-order-쿼드트리)
7. [예산 맞추기 (Budget fitting)](#7-예산-맞추기-budget-fitting)
8. [타일에 그리기](#8-타일에-그리기)
9. [그림자 캐시](#9-그림자-캐시)
10. [셰이더에서 타일 읽기: 타일 → UV](#10-셰이더에서-타일-읽기-타일--uv)
11. [CPU ↔ GPU 데이터 레이아웃](#11-cpu--gpu-데이터-레이아웃)
12. [디버그 뷰](#12-디버그-뷰)
13. [알려진 한계와 다음 단계](#13-알려진-한계와-다음-단계)
14. [코드 지도](#14-코드-지도)
15. [용어 정리](#15-용어-정리)

---

## 1. 그림자 매핑 한 장 요약

**그림자 맵(Shadow map)** 은 광원의 시점에서 렌더링한 깊이 이미지입니다. 각 텍셀에는 "그 방향으로 광원에서 가장 가까운 표면까지의 깊이" 가 저장됩니다.

월드 위치 `P` 에 있는 픽셀을 셰이딩할 때:

1. `P` 를 광원의 ViewProjection 행렬로 변환 → 클립 공간 → `w` 로 나눔 → NDC
2. NDC의 `x, y` 는 그림자 맵의 **어디를** 볼지, NDC의 `z` 는 그 픽셀의 **광원 기준 깊이** 입니다.
3. 비교: 저장된 깊이가 픽셀 깊이보다 작으면, 픽셀과 광원 사이에 무언가 있다는 뜻 → **그림자**

```
   light ●
          \
           \   저장된 깊이 (가장 가까운 표면)
            ■────────── 가리는 물체 (occluder)
             \
              \
               ● P   받는 쪽 깊이 > 저장된 깊이 → 그림자 안
```

비교는 하드웨어가 해 줍니다. **비교 샘플러**(`ComparisonLinear`, `D3D11_COMPARISON_LESS_EQUAL`)로 `SampleCmpLevelZero` 를 호출하면 `ReceiverDepth <= StoredDepth` 인 곳은 `1`(빛 받음), 아니면 `0` 을 돌려줍니다. 선형 필터링이 켜져 있으면 샘플 위치 주변 **2×2 텍셀** 에 대해 각각 비교한 뒤 섞어 줍니다. 이것이 가장 싼 부드러운 그림자 경계("하드웨어 PCF")입니다.

**Bias.** 깊이는 정밀도와 해상도가 제한되어 있어서, 표면이 자기 자신을 가려 얼룩덜룩한 무늬가 생깁니다(shadow acne). 이 엔진에는 두 가지 대책이 있습니다.

- **그림자 패스의 깊이 bias** (`LightShadowShader.hlsl`): 기록되는 깊이를 `ShadowDepthBias + ShadowSlopeBias · fwidth(z)` 만큼 뒤로 밉니다. 비스듬히 보이는 표면일수록 `fwidth(z)` 가 커져 bias도 커집니다.
- **조명 패스의 Normal offset + Receiver plane bias**: 비교하기 전에 받는 쪽 위치를 법선 방향으로 약 텍셀 하나 크기만큼 옮기고, 필터 탭마다 표면 기울기에 맞춰 비교 깊이를 보정합니다 ([§10.8](#108-normal-offset-텍셀-하나의-월드-크기), [§10.9](#109-receiver-plane-bias-기울기-단위-맞추기)).

---

## 2. 왜 아틀라스인가

### 여러 그림자 맵이 하나의 리소스 안에 있어야 한다

조명 셰이더는 픽셀마다 라이트 목록을 돌면서, 라이트마다 다른 그림자 맵을 골라 읽어야 합니다. 그런데 D3D11 / Shader Model 5.0에서는 **별개의 `Texture2D` 여러 개를 런타임 값으로 인덱싱할 수 없습니다.** 그래서 모든 그림자 맵은 **하나의** 리소스 안에 있고, "어디에 있는지" 는 데이터로 알려 줘야 합니다.

| 담는 방식 | 그림자 맵 하나의 주소 | 제약 |
|---|---|---|
| `Texture2DArray` | 슬라이스 번호 | 모든 슬라이스 크기가 같아야 함 |
| **아틀라스** (큰 `Texture2D` 하나) | 그 안의 사각형 영역 | 크기가 달라도 됨 |

### 아틀라스가 주는 것

- **라이트마다 자기 해상도** 의 타일을 가질 수 있습니다 (`ShadowMapResolution` 설정이 실제로 반영됨).
- 텍스처 생성/삭제가 런타임에 없습니다. 처음에 한 번 만들고, "할당" 은 정수 계산뿐입니다.
- 모든 면이 같은 깊이 타깃에 그려지므로 면마다 바뀌는 것은 뷰포트뿐입니다. 조명 패스에서도 `t14` 하나만 바인딩하면 됩니다.
- 공간이 부족하면 먼저 타일을 줄이고, 최후에만 그림자를 뺍니다 ([§7](#7-예산-맞추기-budget-fitting)).
- 타일이 없는 라이트는 셰이더에서 명시적으로 감지해 그림자 없이 그립니다 ([§10.10](#1010-타일이-없으면--빛-받음)).

GPU가 래스터라이즈하는 픽셀 수는 그대로이므로, 이득은 주로 **CPU/드라이버 쪽** 과 **"서로 다른 크기의 그림자 맵을 한 패스에서 읽을 수 있다"** 는 점입니다.

### "Point Light는 큐브맵이 필요하다" — 여기서는 아님

Point Light 그림자는 흔히 `TextureCube` 에 저장하고, 하드웨어가 방향 벡터로 면을 고릅니다. 하지만 큐브 뷰는 2D 텍스처의 일부 사각형 위에 만들 수 없으므로 아틀라스에 넣을 수 없습니다.

이 엔진은 Point Light를 **6개의 원근 뷰**(−X, +X, −Y, +Y, −Z, +Z)로 그리고, 면 선택은 셰이더가 직접 합니다 ([§10.11](#1011-point-light-면-고르기)). GPU 입장에서 Point Light는 Spot Light 6개와 같습니다. 2D 타일 6개는 다른 타일처럼 아틀라스에 들어갑니다. (Unity URP도 같은 방식으로 Point/Spot을 "additional lights" 아틀라스 하나에 넣습니다.)

---

## 3. 구조 한눈에 보기

```
                      ┌──────────────────────────────┐
 Directional (CSM) ──►│ Texture2DArray  t13          │  뷰포트마다, 슬롯 기반
                      └──────────────────────────────┘
                      ┌──────────────────────────────┐
 Spot + Point ───────►│ Local Atlas  8192²  t14      │  프레임당 한 번, 타일 기반 (이 문서)
                      └──────────────────────────────┘
```

| 항목 | 값 |
|---|---|
| 텍스처 | `FShadowRenderer::LocalAtlas`, `FTexture2D` 하나 |
| 크기 | `LocalAtlasSize = 8192` (8192² × 4바이트 = **256 MiB**) |
| 포맷 | `DXGI_FORMAT_R24G8_TYPELESS` |
| 깊이 뷰 (쓰기) | `D24_UNORM_S8_UINT` |
| 셰이더 뷰 (읽기) | `R24_UNORM_X8_TYPELESS`, `t14` 에 바인딩 |
| 가장 작은 타일 | `LocalAtlasMinTileSize = 256` |
| 수명 | 캐시 꺼짐: 매 프레임 전체 클리어 후 다시 그림 / 캐시 켜짐: 타일이 프레임을 넘어 유지됨 ([§9](#9-그림자-캐시)) |

**왜 `R24G8_TYPELESS` 인가?** Typeless 포맷은 같은 메모리를 두 개의 *뷰* 가 다르게 해석할 수 있게 합니다. 깊이 뷰는 24비트 깊이 + 8비트 스텐실로 쓰고, 셰이더 뷰는 24비트 깊이를 0–1 값으로 읽습니다. 그래서 같은 텍스처가 그림자 패스에서는 깊이 타깃, 조명 패스에서는 읽을 수 있는 텍스처가 됩니다.

**왜 Directional 배열과 분리했나?** 수명이 다릅니다. Local 라이트는 프레임당 한 번 그리고 모든 뷰포트가 공유합니다. Directional 캐스케이드는 각 뷰포트의 카메라에 따라 달라서 뷰포트마다 다시 그립니다.

---

## 4. 한 프레임의 흐름

```
UEditorEngine::RenderViewports
│
├─ FShadowRenderer::BeginFrame
│     t13, t14 언바인드 · 요청 목록 비우기
│     캐시 꺼짐 → 아틀라스 전체를 깊이 1.0으로 클리어 + 할당기/캐시 리셋
│     그림자가 방금 다시 켜짐 → 할당기/캐시 리셋
│
├─ 월드마다 (에디터, PIE):
│     AllocateDirectionalSlots(Scene)        태양용 배열 슬롯 (여기서 다루지 않음)
│     GatherLocalRequests(Scene, Views)      켜진 Spot/Point 중 어떤 뷰에든 보이는 것만
│                                            {Scene, Light, Size, ViewCount, LightId} 요청 추가
│
├─ RenderLocalAtlas
│     1. FitRequestsToBudget                 총 면적이 맞을 때까지 줄이기/빼기       (§7)
│     2. 이번 프레임에 본 라이트 표시 → 안 보인 라이트의 타일 반납   (SweepUnseenCacheEntries)
│     3. stable_sort: Size 큰 것부터                                              (§6.5)
│     4. 크기/면 수가 바뀐 라이트의 타일 반납
│     5. 타일이 없는 라이트에 TryAllocate (실패하면 RepackLocalAtlas)             (§6)
│     6. 면마다: 재사용 가능하면 건너뜀, 아니면
│           아틀라스 바인드(클리어 X) · 뷰포트 = 타일 · (캐시 켜짐) ClearTile · 캐스터 그리기 (§8)
│     7. EndRenderPass (아틀라스를 깊이 타깃에서 해제)
│
└─ 뷰포트마다:
      FSceneRenderer::InitViews
         라이트 렌더 데이터 복사 · GetAtlasTile → TileToAtlasRect → ShadowAtlasRect  (§10.2)
         라이트 버퍼 업로드
      FRenderer::SetupView
         FrameConstants.LocalAtlasTexelSize = 1 / 8192                            (§10.7)
      메인 패스 (t14 = Local Atlas)
         픽셀마다, 라이트마다:
           Point: 면 고르기 · Spot: 뷰 하나                                         (§10.11)
           Normal offset → 투영 → Local UV → 범위 검사 → Atlas UV → 클램프하며 필터  (§10)
```

D3D11의 **읽기/쓰기 충돌 규칙**(같은 리소스를 셰이더 입력과 깊이 타깃으로 동시에 바인딩할 수 없음. D3D11은 경고를 남기고 한쪽을 조용히 해제함) 때문에 순서가 중요한 곳이 두 군데 있습니다.

- `BeginFrame` 은 아틀라스를 클리어하고 그리기 전에 `t14` 를 언바인드합니다.
- `RenderLocalAtlas` 는 메인 패스가 `t14` 에 아틀라스를 바인딩하기 전에 `EndRenderPass()` 로 끝납니다.

---

## 5. 데이터 모델

```cpp
// Render/ShadowAtlasAllocator.h — 아틀라스 안의 정사각형 영역
struct FShadowAtlasTile
{
	uint32 X = 0;      // 픽셀 좌표 (Index에서 계산해 둔 값)
	uint32 Y = 0;
	uint32 Size = 0;   // 0이면 "타일 없음"
	uint32 Index = 0;  // Z-order 번호 (할당기 내부의 진짜 기준)
};

// Render/ShadowRenderer.h — 라이트 하나가 이번 프레임에 요청하는 것
struct FShadowRequest { FScene* Scene; FLightSceneProxy* Light; uint32 Size; uint32 ViewCount;
                        uint32 LightId; uint32 CullViewOffset; uint32 CullViewCount; };

// Render/ShadowRenderer.h — 라이트 하나가 가진 타일들 (프레임을 넘어 유지)
struct FShadowCacheEntry
{
	uint32 Size, ViewCount;
	FShadowAtlasTile Tiles[6];          // 면 i 의 타일
	FShadowViewCapture Captures[6];     // 면 i 를 마지막으로 그렸을 때의 행렬/bias
	uint64 LastSeenFrame;
	bool bTilePlacementChanged;         // 이번 프레임에 자리가 바뀌었으면 무조건 다시 그림
};
```

| 개념 | 위치 | 누가 씀 | 수명 |
|---|---|---|---|
| **Shadow view** — 깊이 이미지 하나를 그리는 카메라 (Spot: 1, Point: 6) | `FShadowMapInfo::ShadowViews` | `FLightSceneProxy::Update()` | 컴포넌트로부터 매 프레임 갱신 |
| **Request** — 라이트의 요청: 크기 × 면 수 | `FShadowRenderer::LocalRequests` | `GatherLocalRequests` | 한 프레임 |
| **Tile** — 아틀라스 안의 픽셀 영역 | `FShadowCacheEntry::Tiles`, `ShadowCaches[LightId]` | `RenderLocalAtlas` | 라이트가 계속 보이고 크기가 같으면 유지 |
| **Rect** — 타일을 아틀라스 UV로 바꾼 값, GPU로 감 | 라이트 렌더 데이터의 `ShadowAtlasRect` | `FSceneRenderer::InitViews` | 뷰 하나 |

설계상 결정:

- **타일은 `LightId` 로 찾습니다.** 렌더러 밖에서는 `GetAtlasTile(LightId, Face, OutTile)` 로만 읽습니다. 이번 프레임에 보이지 않은 라이트(`LastSeenFrame != FrameNumber`)나 그림자가 꺼진 경우에는 `false` 를 돌려줍니다.
- **요청은 자기 Scene을 기억합니다.** 캐스터는 라이트가 속한 `FScene` 에서 모아야 하는데, 프록시는 자기 Scene을 모르기 때문입니다.
- **한 라이트의 모든 면은 같은 크기** 를 씁니다 ([§7.3](#73-point-light의-모든-면이-같이-줄어드는-이유)).

---

## 6. 타일 할당: Z-order 쿼드트리

`FShadowAtlasAllocator` (`Render/ShadowAtlasAllocator.cpp`) 에 구현되어 있습니다. Z-order 자체는 [ShadowAtlasZOrder.md](ShadowAtlasZOrder.md) 에서 예시와 함께 자세히 설명하므로 여기서는 핵심만 정리합니다.

### 6.1 쿼드트리

타일 크기는 모두 2의 거듭제곱(256 ~ 8192)이고 아틀라스도 2의 거듭제곱(8192)입니다. 아틀라스를 4등분하고, 각 칸을 또 4등분하고 … 하면 크기 `S` 인 타일은 깊이 `log2(8192 / S)` 의 **노드 하나를 정확히** 채웁니다.

| Level | 타일 크기 | 작은 칸(256²) 개수 `CellsAt(Level)` |
|---|---|---|
| 0 | 8192 | 1024 |
| 1 | 4096 | 256 |
| 2 | 2048 | 64 |
| 3 | 1024 | 16 |
| 4 | 512 | 4 |
| 5 | 256 | 1 |

### 6.2 칸(Cell)과 Z-order

아틀라스를 `MinTileSize` = 256 픽셀짜리 **칸(cell)** 으로 나누면 한 변에 32칸, **총 1024칸** 입니다. 칸 번호는 **Z-order** 로 매깁니다. 번호의 비트가 x, y를 번갈아 담습니다.

```
번호 비트:  … y2 x2 y1 x1 y0 x0

 4×4 모서리 부분:       x: 0   1   2   3
                  y: 0    0   1   4   5
                     1    2   3   6   7
                     2    8   9  12  13
                     3   10  11  14  15
```

**핵심 성질:** `4ᵏ` 의 배수에서 시작하는 연속된 `4ᵏ` 개 번호는 `2ᵏ × 2ᵏ` 정사각형을 이룹니다. 그래서 쿼드트리 블록 하나를 **시작 번호 하나** 로 부를 수 있고, 부모·자식·형제 계산이 덧셈과 비트 연산으로 끝납니다.

### 6.3 할당: 레벨별 빈 칸 목록 (Buddy allocator)

할당기는 Level마다 빈 블록의 시작 번호 목록 `FreeLists[Level]` 을 가집니다. `Reset` 직후에는 `FreeLists[0] = { 0 }` (아틀라스 전체) 하나뿐입니다.

```
Allocate(S):
    Want = LevelOf(S)
    Want 레벨부터 위(더 큰 블록)로 올라가며 빈 목록이 있는 레벨 Found 를 찾는다
    없으면 → 실패 ({}, Size == 0)
    Index = PopLowest(Found)                   // 번호가 가장 작은 블록
    Found 에서 Want 까지 한 단계씩 쪼갠다:
        자식 0 은 계속 쪼갤 대상으로 남기고
        자식 1, 2, 3 (Index + i · CellsAt(Level+1)) 은 FreeLists[Level+1] 에 넣는다
    return MakeTile(Index, S)
```

```
Free(Tile):
    Level = LevelOf(Tile.Size), Index = Tile.Index
    while Level > 0:
        Parent = Index & ~(CellsAt(Level-1) - 1)
        형제 3개가 모두 FreeLists[Level] 에 있으면 → 형제를 목록에서 빼고, 부모로 올라가 반복
        아니면 → 중단
    FreeLists[Level] 에 Index 추가
```

- **가장 작은 번호부터** 쓰기 때문에 왼쪽 위부터 차곡차곡 채워지고 조각화가 줄어듭니다.
- **`TryAllocate(Size, Count)`** 는 한 라이트의 모든 면을 한 번에 할당합니다. 하나라도 실패하면 이미 받은 타일을 전부 반납합니다. Point Light는 6면을 다 받거나 하나도 못 받습니다.

### 6.4 번호 → 픽셀: `CompactEvenBits`

X는 번호의 0, 2, 4, … 번 비트이고, Y는 1, 3, 5, … 번 비트입니다.

```
CellX = CompactEvenBits(Index)
CellY = CompactEvenBits(Index >> 1)      // 한 칸 밀면 Y 비트가 짝수 자리로 옴
Pixel = (CellX · 256, CellY · 256)
```

`CompactEvenBits` 는 입력의 `2k` 번 비트를 출력의 `k` 번 비트로 옮깁니다. 마스크 + 시프트 5번으로, 매 단계 비트 사이 간격을 절반으로 줄입니다.

| 단계 | 연산 | 비트 묶음 크기 |
|---|---|---|
| 1 | `v &= 0x55555555` | 1 (짝수 비트만 남김) |
| 2 | `v = (v \| v >> 1) & 0x33333333` | 2 |
| 3 | `v = (v \| v >> 2) & 0x0F0F0F0F` | 4 |
| 4 | `v = (v \| v >> 4) & 0x00FF00FF` | 8 |
| 5 | `v = (v \| v >> 8) & 0x0000FFFF` | 16 (빈틈 없이 모임) |

예: `Index = 256 = 0b1_0000_0000` (8번 비트만 1, 짝수 → X 쪽)
→ `CellX = 16`, `CellY = 0` → 픽셀 **(4096, 0)**, 즉 오른쪽 위 4096 블록의 모서리입니다.

### 6.5 큰 것부터 정렬하는 이유와 Repack

캐시가 꺼져 있으면 매 프레임 빈 아틀라스에 **큰 것부터** 할당합니다. 2의 거듭제곱 크기를 큰 순서로, 가장 작은 번호부터 넣으면 **빈틈이 생기지 않습니다.** 그래서 "총 면적 ≤ 아틀라스 면적" 이면 **반드시 다 들어갑니다.** 예산 맞추기([§7](#7-예산-맞추기-budget-fitting))는 이 성질에 기대고 있습니다.

캐시가 켜져 있으면 타일이 프레임을 넘어 남아 있으므로, 라이트가 사라지거나 크기가 바뀌면서 **조각화** 가 생길 수 있습니다. 그러면 총 면적은 맞는데도 새 라이트가 `TryAllocate` 에 실패할 수 있습니다. 이때는 `RepackLocalAtlas` 가

1. 할당기와 캐시를 모두 리셋하고,
2. (이미 큰 순서로 정렬된) 요청 전체를 다시 할당합니다.

위 성질 때문에 이 재배치는 반드시 성공합니다 (`assert(bFits)`). 다시 배치된 타일은 `bTilePlacementChanged = true` 가 되어 이번 프레임에 다시 그려집니다.

정렬은 `std::stable_sort` 라서 같은 크기는 수집 순서를 유지하고, 레이아웃이 프레임마다 흔들리지 않습니다.

---

## 7. 예산 맞추기 (Budget fitting)

`FShadowRenderer::FitRequestsToBudget` 에 구현되어 있고, **정렬 전에** 호출됩니다.

### 7.1 면적

요청 하나의 면적은 `ViewCount × Size²` 입니다 (`GetRequestArea`, 64비트로 계산). 용량은 `8192² = 67,108,864` 텍셀 = 256² 칸 1024개입니다.

### 7.2 규칙

```
Total = Σ GetRequestArea(request)
while Total > Capacity:
    Size 가 가장 큰 요청을 고른다 (같으면 나중에 수집된 것)
    그 Size > MinTileSize 이면:
        Size /= 2                              // 줄이기
    아니면:                                    // 모두 최소 크기
        마지막 요청을 제거                      // 빼기: 이번 프레임 그 라이트는 그림자 없음
```

이렇게 하는 이유:

- **가장 큰 것을 절반으로.** 절반으로 줄이면 그 요청 면적의 ¾ 가 비워집니다. 한 번에 가장 많이 확보하는 방법입니다. 큰 맵은 디테일에 여유도 있습니다.
- **같으면 나중 것 / 빼기도 마지막.** 수집 순서(씬의 라이트 순서 → 월드 순서)로 결과가 결정되므로 프레임마다 레이아웃이 깜빡이지 않습니다.
- **빼기 전에 줄이기.** 작은 그림자가 없는 그림자보다 거의 항상 낫습니다.

### 7.3 Point Light의 모든 면이 같이 줄어드는 이유

예산 맞추기는 **요청 단위** 로 동작하고, 요청 하나는 라이트 하나이므로 6면은 항상 같은 크기입니다. 의도된 동작입니다.

1. **이음매.** 인접한 면은 큐브 모서리에서 만납니다. 크기가 다르면 모서리에서 그림자 선명도와 필터 폭이 갑자기 바뀝니다.
2. **라이트당 해상도 하나.** bias 설정은 라이트당 해상도 하나를 가정합니다.
3. 카메라가 어디를 보는지 모르면 **어느 면이 더 중요한지 알 수 없습니다.**

면 단위로 최적화하려면 줄이는 게 아니라, 어떤 카메라에도 안 보이는 면을 **건너뛰는** 것이 맞습니다 ([§13](#13-알려진-한계와-다음-단계)).

---

## 8. 타일에 그리기

### 8.1 위치는 뷰포트가 정한다

광원의 투영 행렬은 절두체를 NDC(−1…1)로 보냅니다. 그 다음 **뷰포트 변환** 이 NDC를 픽셀로 보냅니다.

```
pixel.x = Viewport.TopLeftX + (ndc.x + 1) / 2 · Viewport.Width
pixel.y = Viewport.TopLeftY + (1 − ndc.y) / 2 · Viewport.Height
```

뷰포트를 타일로 설정하면 — `SetViewport(Tile.X, Tile.Y, Tile.Size, Tile.Size)` — 절두체 전체가 타일 안에 그려집니다. **투영 행렬은 바뀌지 않습니다.** GPU는 삼각형을 절두체로 클리핑하고 절두체는 정확히 뷰포트 사각형에 대응하므로, 이웃 타일에는 아무것도 그려지지 않습니다.

> 셰이더의 Local UV 계산([§10.3](#103-1단계-월드--local-uv))은 바로 이 뷰포트 변환을 **0–1 범위로** 그대로 따라 한 것입니다.

### 8.2 클리어

D3D11의 `ClearDepthStencilView` 에는 **사각형 인자가 없습니다.** 항상 텍스처 전체를 지웁니다.

- **캐시 꺼짐:** `BeginFrame` 에서 아틀라스 전체를 깊이 1.0으로 **한 번** 클리어하고, 모든 타일을 새로 그립니다.
- **캐시 켜짐:** 다른 타일은 지우면 안 되므로, 다시 그리는 타일만 `ClearTile()` 로 지웁니다. 화면 전체를 덮는 삼각형 하나를 깊이 1.0으로 그리면(깊이 테스트 Always, 색 쓰기 없음) **뷰포트 = 타일** 영역만 깊이 1.0이 됩니다.
- 어느 경우든 타일은 `BindRenderPassNoClear(nullptr, LocalAtlas, 8192, 8192)` → `SetViewport(tile)` 순서로 그립니다. `BeginRenderPass(DSV, …)` 는 이전 타일을 전부 지우므로 아틀라스에 쓰면 안 됩니다.

### 8.3 면마다 그리기

```
그림자 타일을 가진 요청마다, 면마다:
    재사용 가능하면 건너뜀 (§9)
    BindRenderPassNoClear(nullptr, LocalAtlas, 8192, 8192)
    SetViewport(Tile.X, Tile.Y, Tile.Size, Tile.Size)
    캐시 켜짐 → ClearTile()
    ViewContext = MakeViewContext(Location, ShadowView, Tile.Size, Tile.Size)
    DrawShadowView(Renderer, *Request.Scene, ViewContext, Bias, SlopeBias, CasterCullViews, CullViewCount)
    Captures[Face] = { ViewProjection, Bias, SlopeBias }
EndRenderPass()
```

- `ViewContext` 에는 **타일 크기** 를 넘깁니다. 실제로 그 해상도로 그려지기 때문입니다.
- `CasterCullViews` 는 그 라이트가 속한 월드의 카메라 뷰들입니다. 캐스터 수집 시 카메라와 무관한 캐스터를 걸러 내는 데 씁니다.

---

## 9. 그림자 캐시

캐시가 켜져 있으면(`bCacheEnabled`) 타일 위치와 내용이 프레임을 넘어 유지되고, 바뀐 면만 다시 그립니다.

면을 **다시 그리는 조건** (`bDraw`):

1. 캐시가 꺼져 있음, 또는
2. 이번 프레임에 타일 자리가 바뀜 (`bTilePlacementChanged`: 새로 할당 / Repack), 또는
3. `ShouldRedrawFace` 가 `true`:
   - 그 면의 ViewProjection, ShadowBias, ShadowSlopeBias 가 마지막으로 그렸을 때(`Captures[Face]`)와 다름 (라이트가 움직였거나 설정이 바뀜)
   - 또는 이번 프레임에 변한 캐스터의 AABB(`Scene.ShadowCasterChanges`) 중 하나가 그 면의 절두체 안에 있음

타일 수명:

- 이번 프레임에 요청되지 않은 라이트 → `SweepUnseenCacheEntries` 가 타일을 반납하고 엔트리를 지웁니다.
- 크기나 면 수가 바뀐 라이트 → 타일을 반납하고 새로 할당합니다.

통계: `ShadowStats.TotalFaces` (사용 중인 면), `ShadowStats.ReusedFaces` (다시 그리지 않은 면).

---

## 10. 셰이더에서 타일 읽기: 타일 → UV

`StaticMeshUberLit.hlsl` 의 `MakeAtlasTile`, `ToAtlasUV`, `SampleAtlasCmp`, `SampleAtlasDepth`, `CalculateAtlasPCF`, `CalculateAtlasPoissonPCF`, `CalculateAtlasPCSS`, `SampleLocalShadow`, `ShadowNormalOffset` 에 구현되어 있습니다.

### 10.1 큰 그림: 좌표계 다섯 단계

픽셀 하나가 아틀라스의 텍셀 하나를 찾아가는 길은 다음과 같습니다.

```
 월드 위치 P
   │  mul(float4(P, 1), LightVP)                         (라이트 행렬)
   ▼
 Light Clip  (x, y, z, w)
   │  / w
   ▼
 Light NDC   x, y ∈ [−1, 1],  z ∈ [0, 1]
   │  x·0.5 + 0.5,  0.5 − y·0.5                          (뷰포트 변환을 0–1로)
   ▼
 Local UV    ∈ [0, 1]  — "이 라이트의 그림자 맵 한 장" 기준
   │  LocalUV · Rect.zw + Rect.xy                        (타일 위치/크기)
   ▼
 Atlas UV    ∈ [0, 1]  — "8192² 아틀라스 전체" 기준
   │  clamp(AtlasUV, ClampMin, ClampMax)                 (이웃 타일 침범 방지)
   ▼
 SampleCmpLevelZero(LocalShadowAtlas, ...)
```

여기서 핵심은 **UV가 두 종류** 라는 점입니다.

| | Local UV | Atlas UV |
|---|---|---|
| 기준 | 라이트(면) 하나의 그림자 맵 | 아틀라스 텍스처 전체 |
| (0, 0) | 그 타일의 왼쪽 위 | 아틀라스의 왼쪽 위 |
| (1, 1) | 그 타일의 오른쪽 아래 | 아틀라스의 오른쪽 아래 |
| 누가 아나 | 라이트 행렬 (투영 결과가 바로 이것) | 텍스처 샘플러 |
| 쓰는 곳 | 범위 검사, 월드 크기 ↔ UV 변환 (PCSS, normal offset) | 실제 샘플링, 필터 탭 오프셋 |

라이트의 투영 행렬은 자기가 아틀라스 어디에 있는지 모릅니다. 행렬은 "그림자 맵 한 장 전체" 에 대해 계산된 그대로이고, **"그 한 장이 아틀라스 어디에 있는지" 는 `Rect` 가 마지막에 붙여 줍니다.**

### 10.2 CPU: 타일 → Rect

셰이더는 픽셀이 아니라 UV로 일합니다. `FSceneRenderer::InitViews` 가 면마다 `GetAtlasTile` 로 타일을 얻고 `FShadowRenderer::TileToAtlasRect` 로 바꿔 라이트 렌더 데이터의 `ShadowAtlasRect` 에 넣습니다.

```
Rect = ( X / 8192,  Y / 8192,  Size / 8192,  Size / 8192 )
         offset.x   offset.y   scale.x       scale.y
타일 없음 → (0, 0, 0, 0)
```

| 타일 (X, Y, Size) | Rect |
|---|---|
| (0, 0, 2048) | (0, 0, 0.25, 0.25) |
| (4096, 0, 1024) | (0.5, 0, 0.125, 0.125) |
| (4096, 2048, 1024) | (0.5, 0.25, 0.125, 0.125) |

- `Rect.xy` = 타일 왼쪽 위 모서리가 아틀라스 UV로 어디인지
- `Rect.zw` = 타일 한 변이 아틀라스 UV로 얼마나 긴지

Point Light는 면마다 하나씩 `float4 ShadowAtlasRect[6]`, Spot Light는 `float4 ShadowAtlasRect` 하나입니다.

### 10.3 1단계: 월드 → Local UV

```hlsl
float4 LightClip = mul(float4(WorldPosition, 1.0f), LightVP);
float3 LightNDC  = LightClip.xyz / LightClip.w;
float2 LocalUV   = float2(LightNDC.x * 0.5f + 0.5f, 0.5f - LightNDC.y * 0.5f);
```

- NDC x는 −1(왼쪽) ~ +1(오른쪽) → `x·0.5 + 0.5` 로 0 ~ 1
- NDC y는 −1(아래) ~ +1(위)인데, 텍스처 UV의 v는 **아래로** 커집니다 → `0.5 − y·0.5` 로 **뒤집음**
- NDC z는 그대로 **비교할 깊이** (`ReceiverDepth`) 로 씁니다.

이 식은 [§8.1](#81-위치는-뷰포트가-정한다)의 뷰포트 변환에서 `TopLeft = 0`, `Width = Height = 1` 로 둔 것과 같습니다. 그림자를 **그릴 때** 와 **읽을 때** 가 같은 변환을 쓰기 때문에 정확히 같은 텍셀을 가리킵니다.

### 10.4 2단계: 범위 검사 — 변환하기 **전에**

```hlsl
bool IsInsideLocalAtlas = all(LocalUV >= 0.0f) && all(LocalUV <= 1.0f)
                       && LightNDC.z >= 0.0f && LightNDC.z <= 1.0f;
if (!IsInsideLocalAtlas) return 1.0f;   // 그림자 맵 밖 → 빛 받음
```

검사는 **반드시 Local UV에서** 합니다. Atlas UV로 바꾼 뒤에는 "타일 밖" 이 곧 "다른 라이트의 타일 안" 이 되어, 범위를 벗어났는지 알 수 없게 됩니다.

### 10.5 3단계: Local UV → Atlas UV

```hlsl
float2 ToAtlasUV(float2 LocalUV, FAtlasTile Tile)
{
    return LocalUV * Tile.Rect.zw + Tile.Rect.xy;
}
```

"타일 크기만큼 줄이고(`× Rect.zw`), 타일 위치로 옮긴다(`+ Rect.xy`)" 입니다.

#### 예시로 따라가 보기

타일 `(X=4096, Y=2048, Size=1024)` → `Rect = (0.5, 0.25, 0.125, 0.125)`
어떤 픽셀을 라이트 행렬로 투영했더니 `LightNDC = (0.2, −0.4, 0.73)` 이었다고 합시다.

| 단계 | 계산 | 결과 |
|---|---|---|
| Local UV | (0.2·0.5 + 0.5,  0.5 − (−0.4)·0.5) | **(0.6, 0.7)** |
| 범위 검사 | 0 ≤ 0.6, 0.7 ≤ 1, 0 ≤ 0.73 ≤ 1 | 통과 |
| Atlas UV | (0.6·0.125 + 0.5,  0.7·0.125 + 0.25) | **(0.575, 0.3375)** |
| 아틀라스 픽셀 | (0.575·8192,  0.3375·8192) | **(4710.4, 2764.8)** |
| 검산 | (4096 + 0.6·1024,  2048 + 0.7·1024) | (4710.4, 2764.8) ✓ |

즉 "타일 안에서 가로 60%, 세로 70% 지점" 이 아틀라스 전체에서는 (4710.4, 2764.8) 픽셀이 됩니다. 비교할 깊이는 `0.73` 입니다.

### 10.6 4단계: 샘플을 타일 안에 가두기 (클램프)

텍스처 배열에서는 슬라이스 가장자리가 곧 텍스처 가장자리라서 샘플러의 address mode가 처리해 줬습니다. 아틀라스에서는 바로 옆 텍셀이 **다른 라이트** 의 것입니다. 그런데 필터는 샘플 위치 주변을 읽습니다.

- 비교 샘플 하나도 **2×2** 텍셀을 섞고,
- 5×5 PCF는 2텍셀, Poisson/PCSS는 최대 약 12텍셀까지 바깥을 읽습니다.

막지 않으면 타일 가장자리 근처의 샘플이 이웃의 깊이를 읽습니다("shadow bleeding"). 그래서 **모든** 샘플은 `SampleAtlasCmp` / `SampleAtlasDepth` 를 거쳐 다음 범위로 클램프됩니다.

```hlsl
FAtlasTile MakeAtlasTile(float4 Rect)
{
    Tile.Rect     = Rect;
    Tile.ClampMin = Rect.xy           + 0.5 * LocalAtlasTexelSize;
    Tile.ClampMax = Rect.xy + Rect.zw - 0.5 * LocalAtlasTexelSize;   // LocalAtlasTexelSize = 1 / 8192
}
```

**왜 정확히 반 텍셀인가.** 텍셀 좌표로 보면, `u` 에서의 선형 샘플은 `floor(u·W − 0.5)` 번 텍셀과 그 `+1` 번 텍셀을 읽습니다 (`W` = 8192).
- `u ≥ (X + 0.5) / W` 이면 첫 텍셀 `≥ X`
- `u ≤ (X + S − 0.5) / W` 이면 두 번째 텍셀 `≤ X + S − 1` (또는 가중치 0)

따라서 2×2 블록 전체가 타일 안에 머뭅니다.

앞의 예시 타일이라면: `ClampMin = (0.5 + 0.000061, 0.25 + 0.000061)`, `ClampMax = (0.625 − 0.000061, 0.375 − 0.000061)`.

PCSS의 blocker 탐색은 클램프 대신 범위 밖 샘플을 **건너뜁니다** (`continue`). 같은 텍셀을 여러 번 읽어 blocker 평균이 치우치는 것을 막기 위해서입니다.

### 10.7 단위 맞추기: 텍셀 하나는 어디서나 같은 크기

아틀라스에서는 어떤 타일이든 **텍셀 하나 = 아틀라스 텍셀 하나** 입니다. 1024 타일의 텍셀과 256 타일의 텍셀은 아틀라스 위에서 같은 크기이고, 타일 크기 차이는 "텍셀이 몇 개 있느냐" 로 나타납니다.

| 양 | 식 | 예 (Size = 1024) |
|---|---|---|
| 텍셀 하나 (Atlas UV) | `LocalAtlasTexelSize.x = 1 / 8192` | 0.000122 |
| 타일 한 변의 텍셀 수 | `TileTexels = Rect.z / LocalAtlasTexelSize.x` | 0.125 · 8192 = 1024 |
| 텍셀 하나 (Local UV) | `LocalAtlasTexelSize.x / Rect.z` = `1 / TileTexels` | 1 / 1024 |
| Local UV 거리 → 텍셀 | `d_local · TileTexels` | |
| 텍셀 → Atlas UV 거리 | `texels · LocalAtlasTexelSize.x` | |
| Local UV 거리 → Atlas UV 거리 | `d_local · Rect.z` | |

필터에서의 적용:

- **PCF**: 탭 오프셋 `(x, y) · LocalAtlasTexelSize` (Atlas UV) — 한 칸에 실제 텍셀 하나.
- **Poisson PCF**: 반경 `2 · LocalAtlasTexelSize.x`. 회전 해시는 `floor(AtlasUV / LocalAtlasTexelSize)` (텍셀마다 다른 의사 난수 각도).
- **PCSS**: 월드 크기 계산은 모두 **Local UV** 에서 하고, 텍셀을 거쳐 Atlas UV로 바꿉니다.

```
ReceiverDistance = LinearPerspectiveDepth(ReceiverDepth, n, f)       // n·f / (f − d·(f − n))
LightRadiusUV    = 0.5 · ProjScale · LightRadius / n                 // near 평면에서 본 광원 크기 (Local UV)
SearchRadiusUV   = LightRadiusUV · (ReceiverDistance − n) / ReceiverDistance
SearchTexels     = min(SearchRadiusUV · TileTexels, 12)              // Local UV → 텍셀
blocker 탐색      : SearchTexels · TexelSize (Atlas UV) 안에서 Poisson 32개, SampleAtlasDepth
PenumbraWorld    = LightRadius · (ReceiverDistance − AvgBlocker) / AvgBlocker     // 닮은 삼각형
PenumbraUV       = 0.5 · ProjScale · PenumbraWorld / ReceiverDistance             // 월드 → Local UV
FilterTexels     = clamp(PenumbraUV · TileTexels, 2, 4)
return 반경 FilterTexels · TexelSize 의 Poisson PCF
```

`ProjScale` 은 투영 행렬의 Y 스케일 `cot(halfFOV)` 입니다. 거리 `z` 에 있는 월드 길이 `L` 은 NDC로 `ProjScale · L / z`, UV로는 그 절반을 차지합니다. **이 관계는 Local UV(그림자 맵 한 장) 기준** 이므로, 텍셀 수로 바꿀 때 `TileTexels` 를 곱합니다.

### 10.8 Normal offset: 텍셀 하나의 월드 크기

받는 쪽 위치를 법선 방향으로 "텍셀 하나의 월드 크기" 정도 밀어 acne를 줄입니다.

```hlsl
float TexelSize = LocalAtlasTexelSize.x / max(Light.ShadowAtlasRect.z, 0.0001f);  // = 1 / TileTexels (Local UV)
float3 Offset   = ShadowNormalOffset(Position, OriginalNormal, OriginalNdotL, Light.ViewProjection, TexelSize);
```

```hlsl
float TexelWorldSize(matrix VP, float ClipW, float TexelSize)
{
    // NDC 로 1 만큼 = 월드로 ClipW / Scale 만큼,  UV 는 NDC 의 절반 → 월드/UV = 2·ClipW / Scale
    return 2.0f * TexelSize * ClipW / min(ScaleX, ScaleY);
}
// Offset = normalize(Normal) · TexelWorldSize · sinθ · 2
```

- `TexelSize` 는 **Local UV 단위의 텍셀 하나** 여야 합니다. `TexelWorldSize` 가 투영 행렬의 스케일(= 그림자 맵 한 장 기준)로 UV를 월드 길이로 바꾸기 때문입니다.
- 그래서 `LocalAtlasTexelSize.x / Rect.z` 로 **타일이 실제로 받은 해상도** 를 씁니다. 예산 맞추기로 타일이 작아졌다면 텍셀이 커지고 offset도 자동으로 커집니다.
- `sinθ` 는 표면이 빛에서 비스듬할수록 커집니다 (비스듬할수록 offset이 더 필요).

### 10.9 Receiver plane bias: 기울기 단위 맞추기

필터 탭마다 "표면이 기울어져 있으니 이 탭 위치에서는 비교 깊이가 이만큼 달라야 한다" 를 보정합니다. `ComputeRPDBdZduv` 는 `dz/du`, `dz/dv` 를 구하는데, 이 값은 NDC에서 나왔으므로 **Local UV 기준** 입니다. 하지만 탭 오프셋(`DeltaUV`)은 **Atlas UV** 입니다.

```hlsl
float3 ReceiverBias = float3(ComputeRPDBdZduv(...), ComputeTapConeSlope(...));
ReceiverBias.xy /= Tile.Rect.zw;   // dz/d(LocalUV) → dz/d(AtlasUV)
ReceiverBias.z  /= Tile.Rect.z;
```

Atlas UV로 `Δ` 움직이면 Local UV로는 `Δ / Rect.z` 움직인 것입니다. 그래서 기울기를 `Rect.z` 로 나눠 두면 이후 필터 코드는 Atlas UV 오프셋을 그대로 넣어도 맞는 깊이 보정을 얻습니다.

### 10.10 타일이 없으면 → 빛 받음

```hlsl
if (AtlasRect.z == 0) return 1.0f;
```

예산 맞추기에서 빠졌거나 어떤 이유로든 타일이 없는 라이트는 **그림자 없이** 그립니다. 다른 라이트의 그림자를 잘못 읽는 일은 없습니다.

### 10.11 Point Light: 면 고르기

6개 뷰는 −X, +X, −Y, +Y, −Z, +Z (면 0–5) 를 바라보는 정사각형 원근 절두체입니다 (FOV 91°: 90°보다 살짝 넓게 겹침). 점이 들어 있는 면은 **라이트 → 점 방향의 가장 큰 성분** 축입니다.

```hlsl
float3 ShadowToLight = Light.Location - Position;
float3 A = abs(ShadowToLight);
uint Face = (A.x >= A.y && A.x >= A.z) ? (ShadowToLight.x > 0 ? 0 : 1)
          : (A.y >= A.z)               ? (ShadowToLight.y > 0 ? 2 : 3)
          :                              (ShadowToLight.z > 0 ? 4 : 5);
```

`ShadowToLight.x > 0` 은 라이트가 점의 +X 쪽에 있다는 뜻이므로, 라이트에서 보면 점은 −X 방향 → 면 0 입니다.

면 선택은 **두 번** 합니다.

1. 원래 위치로 면을 골라, 그 면의 `Rect.z` 로 normal offset 크기를 구하고
2. offset된 위치(`OffsetPosition`)로 **다시** 면을 고릅니다. offset 때문에 점이 면 경계를 넘어갈 수 있기 때문입니다.

그 다음 고른 면의 `ViewProjection[Face]`, `ShadowAtlasRect[Face]` 로 위의 1–4단계를 수행합니다. Spot Light는 뷰가 하나(FOV = 바깥 콘 각도 × 2)라 면 선택이 없습니다.

---

## 11. CPU ↔ GPU 데이터 레이아웃

### 11.1 Structured buffer (라이트) — 빈틈 없는 패킹

라이트 데이터는 `StructuredBuffer<PointLightInfo>` / `<SpotLightInfo>` 로 올라갑니다. Structured buffer는 **4바이트 단위로 빈틈 없이** 패킹되고, 원소 크기는 C++ 쪽 `sizeof` 와 HLSL 구조체가 각각 정합니다. 필드 하나만 달라도 첫 번째 이후 라이트는 모두 엉뚱한 오프셋에서 읽힙니다.

지키는 규칙:

- C++ (`FPointLightRenderData`, `FSpotLightRenderData`, `LightSceneProxy.h`)와 HLSL (`PointLightInfo`, `SpotLightInfo`)은 **같은 커밋에서** 함께 바꿉니다.
- 새 필드는 **뒤에 추가** 해서 기존 오프셋이 움직이지 않게 합니다: `float4 ShadowAtlasRect[6]` (Point), `float4 ShadowAtlasRect` (Spot).

### 11.2 Constant buffer (프레임) — 16바이트 행

`FrameConstants` 는 상수 버퍼라서 필드가 16바이트 경계를 넘으면 안 됩니다. `LocalAtlasTexelSize` (`float2`) 는 `Near`, `Far` 뒤에 붙어 16바이트 행 하나를 채우고, C++ 쪽 `static_assert(sizeof(FFrameConstants) % 16 == 0)` 가 전체 크기를 확인합니다.

### 11.3 바인딩

| 슬롯 | 리소스 |
|---|---|
| `t11` | Point Light (`StructuredBuffer`) |
| `t12` | Spot Light (`StructuredBuffer`) |
| `t13` | Directional 그림자 배열 (`Texture2DArray`) |
| `t14` | **Local Atlas** (`Texture2D<float>`) |
| `s1` | `ComparisonLinear` (`LESS_EQUAL`) |
| `s2` | PCSS blocker 탐색용 깊이 샘플러 |

---

## 12. 디버그 뷰

Details 패널에서 Spot / Point Light를 선택하면:

- **Shadow Map (i)** — 각 면의 타일. `ImGui::Image(LocalAtlasSRV, size, UV0, UV1)` 에 `UV0 = Rect.xy`, `UV1 = Rect.xy + Rect.zw` 를 넣습니다. 셰이더와 같은 `TileToAtlasRect` 를 쓰므로, 여기서 보이는 영역이 곧 셰이더가 읽는 영역입니다.
- **Shadow Local Atlas** — 아틀라스 전체. 빈 영역은 빨간색입니다 (깊이 1.0으로 클리어됨, 셰이더 뷰가 깊이를 빨강 채널에 넣음).

아틀라스는 평범한 `Texture2D` 라서 SRV를 바로 보여 줄 수 있습니다. 배열 슬라이스는 ImGui 셰이더가 배열을 샘플할 수 없어 먼저 복사해야 했습니다 (`CopyShadowSliceForDebug`).

---

## 13. 알려진 한계와 다음 단계

| 항목 | 설명 |
|---|---|
| **Point Light 이음매** | 클램프가 각 면 가장자리에서 필터를 멈추게 하므로 큐브 면이 만나는 곳에 희미한 선이 보일 수 있습니다. 현재 FOV를 91°로 살짝 넓혀 완화하고 있습니다. 정식 해결은 *guard band*: 바깥 `k` 텍셀이 유효한 깊이를 갖도록 `FOV = 2 · atan(S / (S − 2k))` 로 그리고, 샘플링도 같은 투영을 쓰는 것입니다. |
| **레이아웃 `static_assert`** | 라이트 렌더 데이터 구조체의 `sizeof` 와 HLSL 구조체 크기가 같은지 확인하는 `static_assert` 가 아직 없습니다. |
| **깊이 포맷** | `R32_TYPELESS` (32비트 float 깊이, 같은 크기, 더 정밀) 또는 16비트 (메모리 절반). |
| **중요도 기반 크기** | 고정 설정 대신 화면에서 라이트가 차지하는 크기로 타일 크기를 정하기. |
| **안 보이는 면 건너뛰기** | 어떤 카메라에도 안 보이는 면은 타일을 받지 않게 하기. |
| **Repack 비용** | 조각화로 할당이 실패하면 전체를 다시 배치하고 모두 다시 그립니다. 라이트가 자주 생기고 사라지는 장면에서는 캐시 이득이 줄어듭니다. |
| **캐스케이드 아틀라스** | Directional 캐스케이드도 아틀라스로 옮겨 같은 Rect 샘플링 경로를 쓰기. |

---

## 14. 코드 지도

| 영역 | 위치 |
|---|---|
| 타일 할당기 | `Source/Runtime/Render/ShadowAtlasAllocator.h/.cpp` |
| 아틀라스 텍스처, 상수 (`LocalAtlasSize`, `LocalAtlasMinTileSize`) | `Source/Runtime/Render/ShadowRenderer.h`, `FShadowRenderer::Initialize` |
| 프레임 시작 리셋/클리어 | `FShadowRenderer::BeginFrame` |
| 수집 / 예산 / 할당 / 그리기 | `GatherLocalRequests`, `FitRequestsToBudget`, `RenderLocalAtlas`, `DrawShadowView` |
| 캐시 | `FShadowCacheEntry`, `ShouldRedrawFace`, `SweepUnseenCacheEntries`, `RepackLocalAtlas`, `ClearTile` |
| 타일 → Rect | `FShadowRenderer::GetAtlasTile`, `FShadowRenderer::TileToAtlasRect` |
| 뷰마다 Rect 채우기 | `FSceneRenderer::InitViews` |
| 프레임 상수, `t14` 바인딩 | `Source/Runtime/Render/Renderer.cpp` (`FFrameConstants`, `SetupView`) |
| 라이트 렌더 데이터 (C++) | `Source/Runtime/Engine/LightSceneProxy.h` |
| 셰이더 샘플링 | `Resources/Shader/StaticMeshUberLit.hlsl` (`SampleLocalShadow` 와 보조 함수) |
| 그림자 패스 깊이 bias | `Resources/Shader/LightShadowShader.hlsl` |
| 타일 클리어 | `Resources/Shader/ShadowTileClearShader.hlsl` |
| 디버그 미리보기 | `UEditorEngine::Init` 의 provider 람다, `DetailsPanel.cpp` 의 `DrawShadowMapPreviews` |

---

## 15. 용어 정리

| 용어 | 뜻 |
|---|---|
| **아틀라스 (Atlas)** | 작은 이미지 여러 장(여기서는 깊이 맵)을 나란히 담은 큰 텍스처 하나 |
| **Local Atlas** | Spot / Point Light용 8192² 아틀라스 |
| **Shadow view** | 깊이 이미지 하나를 그리는 카메라. Spot: 1, Point: 6 |
| **타일 (Tile)** | 아틀라스 안의 정사각형 픽셀 영역 `(X, Y, Size)` 과 그 Z-order 번호 `Index` |
| **칸 (Cell)** | 가장 작은 할당 단위, `MinTileSize²` = 256² |
| **요청 (Request)** | 라이트 하나의 이번 프레임 요청: 면당 `Size` × `ViewCount` |
| **Rect** | 타일을 아틀라스 UV로 나타낸 것: `(offset.xy, scale.zw)` |
| **Local UV** | 타일(그림자 맵 한 장) 하나 기준 0–1. 라이트 행렬이 만들어 내는 좌표, 그림자 수학이 쓰는 좌표 |
| **Atlas UV** | 아틀라스 전체 기준 0–1. 텍스처를 실제로 샘플할 때 쓰는 좌표 |
| **Z-order / Morton order** | x, y 비트를 번갈아 끼워 칸에 번호를 매기는 방식. 쿼드트리를 한 줄로 펼친 것 |
| **Buddy allocator** | 블록을 반(여기서는 4등분)으로 쪼개 할당하고, 형제가 모두 비면 다시 합치는 할당기 |
| **Repack** | 조각화로 할당이 실패했을 때 아틀라스를 비우고 전체를 다시 배치하는 것 |
| **예산 맞추기 (Budget fitting)** | 총 면적이 아틀라스에 맞을 때까지 요청을 줄이거나 빼는 것 |
| **PCF** | Percentage-closer filtering: 여러 깊이 비교의 평균 → 부드러운 경계 |
| **PCSS** | Percentage-closer soft shadows: 가리는 물체와의 거리에 따라 PCF 반경이 바뀜 → 물체에서 멀수록 반그림자가 넓어짐 |
| **Shadow acne** | 깊이 정밀도/해상도 한계로 생기는 얼룩덜룩한 자기 그림자. bias와 normal offset으로 해결 |
| **Shadow bleeding** | 필터가 이웃 타일의 깊이를 읽는 현상. 반 텍셀 클램프로 방지 |
| **Typeless 포맷** | 같은 메모리를 다른 포맷의 뷰로 볼 수 있는 텍스처 포맷 (깊이 뷰 vs 셰이더 뷰) |
