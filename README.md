# GameTechLab\_04

게임테크랩 4기 6개월간의 학습과 팀 프로젝트 기록입니다.

## 주차별 프로젝트

### [Week1 — 버블 슈터](./Week1)

* **주요 구현:** 공 발사와 충돌, 같은 색 공 3개 이상 연결 시 제거, 천장과 연결이 끊긴 공의 낙하를 구현한 버블 슈터 게임.
* **담당 부분:** 맵·공 데이터 구조와 BFS 기반 게임 로직 구현. 공 낙하·바운스 효과 및 델타타임 처리 구현.

### [Week2 — PEPE Engine](./Week2)

* **주요 구현:** 3D 객체 생성·선택·변환, 기즈모 조작과 씬 저장·불러오기를 지원하는 에디터.
* **담당 부분:** ImGui 기반 씬·속성·콘솔 UI, 입력 처리 및 윈도우 리사이징 대응 구현.

### [Week3 — Macaw Engine](./Week3)

* **주요 구현:** 객체 변환 기즈모, 조명, 텍스트·빌보드 렌더링을 구현한 3D 엔진.
* **담당 부분:** 한글 텍스트 렌더링, 빌보드·네임태그 시스템 구현 및 기즈모 회전 처리 개선.

### [Week4 — Podo Engine](./Week4)

* **주요 구현:** 다중 뷰포트, OBJ/MTL 모델 로딩과 에셋 브라우저를 지원하는 3D 엔진.
* **담당 부분:** 최대 4분할 뷰포트와 Splitter, 에디터 설정 저장·복원 및 FPS·메모리 통계 오버레이 구현.

### [Week5 — Hitori Engine](./Week5)

* **주요 구현:** 메시 LOD 생성·선택, BVH 기반 공간 탐색과 GPU 오클루전 컬링 등 렌더링 최적화 기능을 구현한 3D 엔진.
* **담당 부분:** 메시 LOD 자동 생성과 화면상 크기를 기준으로 한 LOD 단계 선택 구현. LOD 품질·전환 임계값 조정, 인덱스 버퍼 재정렬 및 GPU 오클루전 오류 수정.

### [Week6 — Study Engine](./Week6)

* **주요 구현:** 컴포넌트 기반 편집, Play In Editor(PIE), 포인트·스포트 라이트와 안개·FXAA 후처리를 지원
* **담당 부분:** PIE, ExponentialHeightFog, ProjcetileMovementComponent, RotationMovementComponent, SpotLightComponent, FXAA, FWorldContext



### [Week7 — Hitori Engine](./Week7)

* **주요 구현:** point light, directional light, ambient light, spot light 등 다양한 조명, Gouraud·Lambert·Phong 셰이딩 구현, 노멀매핑 구현,  Compute Shader를 활용한 타일 기반 라이트 컬링과 노멀·컬링 결과 시각화 구현.
* **담당 부분:** 4가지 light 구현 및 Light Actor 구조 설계, 탄젠트 공간 기반 노멀 매핑과 World Normal 시각화 구현. 노멀맵 로딩 및 머티리얼 편집 기능 연동

### [Week8 — Hitori Engine](./Week8)

* **주요 구현:** CSM, PCF·PCSS 기반 그림자 렌더링과 다중 페이지 섀도 아틀라스, 그림자 캐싱을 구현
* **담당 부분:** PCSS의 깊이 계산과 소프트 섀도 품질 개선. 광원별 그림자 설정과 최소 탐색 깊이 추가, Rect Light 광원 반경 처리 수정.

### [Week9 — HitoriEngine](./Week9)



* **주요 구현:** “Light, Camera, Action!”을 주제로 시네마틱 연출을 추가한 2인 차량 축구 게임. LuaJIT·Hot Reload·Coroutine과 Delegate 기반 이벤트, Box·Capsule·Sphere 충돌을 지원하며, 카메라 효과·시퀀서·타격 연출과 게임 시작·종료·재시작·스코어를 구현.

* **담당 부분:** BVH 기반 형상 충돌 검사·충돌 응답·Hit/Overlap 이벤트·이동 Sweep 통합과 중력·가속도·마찰·반발 설정 구현. 시퀀스 데이터·재생기·시간 제어를 설계하고, Hit Stop·Slomo 및 카메라 효과의 시퀀서 연동·에디터 미리보기, 차량 Hit Squash와 주행 밸런스 개선.

