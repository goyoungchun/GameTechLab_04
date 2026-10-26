-- 화면 UI 사용 예시: 텍스트, 이미지, 슬라이더.
-- 아무 액터에 붙여 PIE로 확인한다. UI 버퍼는 매 Tick 비워지므로 그리기는 항상 Tick에서 한다.
--
-- 좌표와 크기는 화면 기준 0~1 비율, (0, 0)이 왼쪽 위다.
-- 글자 크기는 1080p 기준 픽셀이고 화면 높이에 비례해 커지고 작아진다.
--
-- 조작
--   W/S 또는 ↑/↓ : 슬라이더 선택
--   A/D 또는 ←/→ : 값 조절 (누르고 있으면 계속 바뀐다)

------------------------------------------------------------
-- 슬라이더
-- 엔진에 슬라이더 위젯은 없다. 값은 Lua 변수로 들고,
-- 입력으로 바꾸고, Rect 몇 개로 그리는 즉시 모드 방식이다.
------------------------------------------------------------

local Sliders = {
    { Label = "Volume",     Value = 0.7, Min = 0,   Max = 1,   Step = 0.5, Format = "%d%%",  Scale = 100 },
    { Label = "Boost Time", Value = 3.0, Min = 1,   Max = 6,   Step = 2.0, Format = "%.1fs", Scale = 1 },
    { Label = "Image Size", Value = 0.2, Min = 0.1, Max = 0.3, Step = 0.1, Format = "%.2f",  Scale = 1 },
}
local SelectedSlider = 1

-- 선택한 슬라이더 값을 키 입력으로 바꾼다. Step은 1초에 바뀌는 양이다.
local function UpdateSliders(dt)
    if IsKeyPressed(Key.W) or IsKeyPressed(Key.Up) then
        SelectedSlider = math.max(1, SelectedSlider - 1)
    end
    if IsKeyPressed(Key.S) or IsKeyPressed(Key.Down) then
        SelectedSlider = math.min(#Sliders, SelectedSlider + 1)
    end

    local Slider = Sliders[SelectedSlider]
    local Delta = 0
    if IsKeyDown(Key.A) or IsKeyDown(Key.Left) then Delta = Delta - 1 end
    if IsKeyDown(Key.D) or IsKeyDown(Key.Right) then Delta = Delta + 1 end
    Slider.Value = math.max(Slider.Min, math.min(Slider.Max, Slider.Value + Delta * Slider.Step * dt))
end

-- 라벨, 막대(배경+채움), 손잡이, 값 글자를 그린다.
local function DrawSlider(Slider, X, Y, Width, bSelected)
    local Ratio = (Slider.Value - Slider.Min) / (Slider.Max - Slider.Min)
    local LabelColor = bSelected and Color.Yellow or Color.White
    local FillColor = bSelected and Color.Orange or Color(0.6, 0.6, 0.6)

    UI.Text(X, Y, Slider.Label, 26, LabelColor)

    -- 막대: Bar 하나로 배경과 채움을 같이 그린다
    local BarY = Y + 0.015
    local BarHeight = 0.012
    UI.Bar(X, BarY, Width, BarHeight, Ratio, FillColor, Color(1, 1, 1, 0.15))

    -- 손잡이: 채움 끝에 작은 사각형
    local KnobWidth = 0.008
    local KnobHeight = 0.03
    UI.Rect(X + Width * Ratio - KnobWidth / 2, BarY + BarHeight / 2 - KnobHeight / 2, KnobWidth, KnobHeight, Color.White)

    -- 값: 막대 오른쪽 끝에 오른쪽 정렬
    local Text = string.format(Slider.Format, Slider.Value * Slider.Scale)
    UI.Text(X + Width, Y, Text, 22, Color(0.85, 0.85, 0.85), Align.Right)
end

------------------------------------------------------------
-- Tick
------------------------------------------------------------

local Time = 0

function Tick(dt)
    Time = Time + dt
    UpdateSliders(dt)

    --------------------------------------------------------
    -- 1. 텍스트
    --------------------------------------------------------
    -- 반투명 패널 위에 제목. y는 글자 기준선(글자 밑줄)이다.
    UI.Rect(0.02, 0.02, 0.96, 0.09, Color(0, 0, 0, 0.5))
    UI.Text(0.5, 0.085, "UI EXAMPLES", 52, Color.White, Align.Center)

    -- 정렬: 같은 x(0.5)를 기준점으로 왼쪽/가운데/오른쪽
    UI.Rect(0.5, 0.14, 0.002, 0.15, Color(1, 1, 1, 0.3))           -- 기준선 표시
    UI.Text(0.5, 0.17, "Align.Left", 26, Color.White, Align.Left)
    UI.Text(0.5, 0.22, "Align.Center", 26, Color.White, Align.Center)
    UI.Text(0.5, 0.27, "Align.Right", 26, Color.White, Align.Right)

    -- 폰트, 색, 깜빡임(알파를 시간으로 바꿈)
    UI.Text(0.03, 0.17, "기본 폰트 (Pretendard)", 28)
    UI.Text(0.03, 0.22, "CookieRun 폰트", 28, Color(0.4, 0.9, 1.0), Align.Left, "CookieRun")
    local Blink = 0.5 + 0.5 * math.sin(Time * 4)
    UI.Text(0.03, 0.27, "PRESS ENTER", 28, Color(1, 1, 1, Blink))

    -- 숫자 표시: string.format으로 만든 문자열
    UI.Text(0.97, 0.17, string.format("TIME %05.1f", Time), 28, Color.Yellow, Align.Right)

    --------------------------------------------------------
    -- 2. 이미지
    --------------------------------------------------------
    local ImagePath = "Assets/hitori.png"
    local Size = Sliders[3].Value                                    -- 슬라이더로 크기 조절
    local ImageY = 0.36

    -- 지정한 크기로 늘려서 (화면 비율에 따라 찌그러질 수 있음)
    UI.Image(0.03, ImageY, Size, Size, ImagePath)
    UI.Text(0.03, ImageY + Size + 0.035, "UI.Image", 22)

    -- 높이만 정하고 폭은 원본 비율 유지 (로고, 아이콘에 적합)
    UI.ImageFit(0.27, ImageY, Size, ImagePath)
    UI.Text(0.27, ImageY + Size + 0.035, "UI.ImageFit", 22)

    -- 이미지 일부만 (왼쪽 위 1/4). 아이콘 시트에서 한 칸만 쓸 때
    UI.ImageUV(0.51, ImageY, Size, Size, ImagePath, 0, 0, 0.5, 0.5)
    UI.Text(0.51, ImageY + Size + 0.035, "UI.ImageUV", 22)

    -- 색 곱하기: 반투명 + 붉게. 선택 안 된 항목을 어둡게 할 때도 같은 방식
    UI.Image(0.75, ImageY, Size, Size, ImagePath, Color(1, 0.5, 0.5, 0.5))
    UI.Text(0.75, ImageY + Size + 0.035, "Color tint", 22)

    -- 원본 크기 읽기
    local W, H = UI.GetImageSize(ImagePath)
    UI.Text(0.97, ImageY - 0.01, string.format("%s  %dx%d", ImagePath, W, H), 18, Color(0.7, 0.7, 0.7), Align.Right)

    --------------------------------------------------------
    -- 3. 슬라이더
    --------------------------------------------------------
    local PanelY = 0.72
    UI.Rect(0.02, PanelY, 0.5, 0.26, Color(0, 0, 0, 0.5))
    UI.Text(0.04, PanelY + 0.045, "SLIDERS  (W/S select, A/D change)", 22, Color(0.8, 0.8, 0.8))
    for i, Slider in ipairs(Sliders) do
        DrawSlider(Slider, 0.04, PanelY + 0.06 + i * 0.06, 0.4, i == SelectedSlider)
    end

    -- 슬라이더 값 사용 예: Boost Time 값으로 게이지 하나를 채운다
    local Boost = (math.sin(Time) * 0.5 + 0.5)
    UI.Text(0.56, PanelY + 0.045, string.format("Boost demo (max %.1fs)", Sliders[2].Value), 22, Color(0.8, 0.8, 0.8))
    UI.Bar(0.56, PanelY + 0.07, 0.4, 0.03, Boost, Color.Orange)
end
