-- 화면 UI Lua 바인딩 확인용 스크립트. 레벨의 아무 액터에 붙여 PIE로 확인한다.
-- UI는 매 Tick 버퍼가 비워지므로 Tick에서 매 프레임 그린다.

local Boost = { 1.0, 1.0 }
local Time = 60

function Tick(dt)
    Time = math.max(0, Time - dt)
    Boost[1] = math.max(0, Boost[1] - dt * 0.1)
    Boost[2] = math.max(0, Boost[2] - dt * 0.2)

    -- 전체 화면: 점수판
    UI.Rect(0.35, 0.02, 0.3, 0.08, Color(0, 0, 0, 0.5))
    UI.Text(0.5, 0.08, string.format("1 : 0   %d", math.ceil(Time)), 48, Color.White, Align.Center)

    -- 플레이어 HUD (분할 화면 영역이 없으면 둘 다 전체 화면 기준으로 겹쳐 보인다)
    for i = 1, 2 do
        UI.BeginPlayer(i)
        UI.Text(0.05, 0.85 - (i - 1) * 0.1, "P" .. i .. " BOOST", 28)
        UI.Bar(0.05, 0.88 - (i - 1) * 0.1, 0.4, 0.03, Boost[i], Color.Orange)
        UI.EndPlayer()
    end

    -- 인자 생략과 폰트 지정
    UI.Text(0.5, 0.5, "한글 스트", 40, Color.Yellow, Align.Center, "CookieRun")
    UI.Text(0.02, 0.98, "default args")

    -- 이미지
    local Checker = "Assets/Checker.png"
    UI.Image(0.02, 0.15, 0.1, 0.1, Checker)                                  -- 늘려서
    UI.ImageFit(0.15, 0.15, 0.1, Checker)                                    -- 원본 비율 유지
    UI.ImageUV(0.3, 0.15, 0.1, 0.1, Checker, 0, 0, 0.5, 0.5)                 -- 왼쪽 위 1/4만
    UI.Image(0.45, 0.15, 0.1, 0.1, Checker, Color(1, 1, 1, 0.4))             -- 반투명
    UI.Text(0.5, 0.21, "on image", 32, Color.Yellow, Align.Center)           -- 이미지 위 글자
    UI.Image(0.6, 0.15, 0.1, 0.1, "Assets/NotExist.png")                     -- 없는 경로: 경고 한 번

    local W, H = UI.GetImageSize(Checker)
    UI.Text(0.02, 0.3, string.format("Checker %dx%d", W, H), 24)
end
