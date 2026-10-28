local HintTextColor = Color(0.8, 0.8, 0.8)
local BackHintText = "뒤로가기 Backspace"

local function DrawBackground()
    UI.Rect(0, 0, 1, 1, Color(0, 0, 0, 0.55))
end

local function DrawBottomHint(HintText)
    UI.Text(0.5, 0.93, HintText, 36, HintTextColor, Align.Center)
end

local function DrawMenuItems(FirstItemY)
    local ItemSpacing = 0.07
    for ItemIndex, ItemLabel in ipairs(Game.Menu.Items) do
        local ItemY = FirstItemY + (ItemIndex - 1) * ItemSpacing
        if ItemIndex == Game.Menu.SelectedIndex then
            UI.Rect(0.38, ItemY - 0.045, 0.24, 0.06, Color(1, 1, 1, 0.15))
            UI.Text(0.5, ItemY, ItemLabel, 40, Color.Yellow, Align.Center)
        else
            UI.Text(0.5, ItemY, ItemLabel, 40, Color.White, Align.Center)
        end
    end
end

local function DrawCountdown()
    if Game.Countdown == nil then return end
    local CountdownText = (Game.Countdown == 0) and "GO!" or tostring(Game.Countdown)
    UI.Text(0.5, 0.55, CountdownText, 160, Color.Yellow, Align.Center, "CookieRun")
end

local function DrawTurnBanner()
    if Game.bIsSuddenDeath then
        UI.Text(0.5, 0.33, "SUDDEN DEATH", 64, Color.Red, Align.Center, "CookieRun")
        return
    end

    local TurnText = string.format("TURN %d / %d", Game.Turn, Game.Config.TotalTurns)
    local AttackText = string.format("P%d ATTACK", Game.AttackerIndex or 0)
    UI.Text(0.5, 0.30, TurnText, 48, Color.White, Align.Center)
    if (Game.AttackerIndex == 1) then
        UI.Text(0.5, 0.37, AttackText, 36, Color.Orange, Align.Center)
    else
        UI.Text(0.5, 0.37, AttackText, 36, Color.Blue, Align.Center)
    end
end

local PlayerColors = { Color.Orange, Color.Blue }

local function DrawTimer()
    local Seconds = math.ceil(Game.RemainTime or 0)
    local TimerColor = (Seconds <= 10) and Color.Red or Color.White
    UI.Text(0.5, 0.08, tostring(Seconds), 56, TimerColor, Align.Center)
end

local function DrawScoreBoard()
    UI.Rect(0.36, 0.01, 0.28, 0.12, Color(0, 0, 0, 0.5))
    UI.Text(0.41, 0.08, tostring(Game.Score[1]), 56, PlayerColors[1], Align.Center)
    UI.Text(0.59, 0.08, tostring(Game.Score[2]), 56, PlayerColors[2], Align.Center)

    if Game.bIsSuddenDeath then
        UI.Text(0.5, 0.115, "SUDDEN DEATH", 20, Color.Red, Align.Center)
    else
        DrawTimer()
        UI.Text(0.5, 0.115, string.format("TURN %d / %d", Game.Turn, Game.Config.TotalTurns), 20, HintTextColor, Align.Center)
    end
end

local function GetBoostRatio(PlayerIndex)
    local Car = Game.Cars and Game.Cars[PlayerIndex]
    if not Car or not Car.GetBoostRatio then return 0 end
    return Car.GetBoostRatio() or 0
end

local function DrawBoostGauge(PlayerIndex, BoostRatio)
    if PlayerIndex == 1 then
        UI.Bar(0.043, 0.928, 0.254, 0.025, BoostRatio, Color(1, 0.57, 0.05))
        UI.Image(0.01, 0.852, 0.30, 0.178, "Assets/Image/LeftBoostGauge.png")
    else
        local BarX, BarY, BarW, BarH = 0.704, 0.928, 0.250, 0.025
        local FillW = BarW * BoostRatio
        UI.Rect(BarX, BarY, BarW, BarH, Color(0, 0, 0, 0.5))
        UI.Rect(BarX + BarW - FillW, BarY, FillW, BarH, Color(0.05, 0.6, 0.95))
        UI.Image(0.69, 0.852, 0.30, 0.178, "Assets/Image/RightBoostGauge.png")
    end
end

local function DrawPlayerPanel(PlayerIndex)
    local TextX = (PlayerIndex == 1) and 0.043 or 0.954
    local TextAlign = (PlayerIndex == 1) and Align.Left or Align.Right

    local BoostRatio = Game.bIsSuddenDeath and 1 or GetBoostRatio(PlayerIndex)
    DrawBoostGauge(PlayerIndex, BoostRatio)

    local BoostLabel = Game.bIsSuddenDeath and "BOOST MAX" or "BOOST"
    UI.Text(TextX, 0.911, string.format("P%d  %s", PlayerIndex, BoostLabel), 30, PlayerColors[PlayerIndex], TextAlign)

    if Game.AttackerIndex then
        local bIsAttacker = (Game.AttackerIndex == PlayerIndex)
        local RoleText = bIsAttacker and "ATTACK" or "DEFEND"
        local RoleColor = bIsAttacker and Color.Red or Color.Green
        UI.Text(TextX, 0.873, RoleText, 50, RoleColor, TextAlign, "CookieRun")
    end
end

local function DrawHUD()
    DrawScoreBoard()
    DrawPlayerPanel(1)
    DrawPlayerPanel(2)
end

local CarSelectHintTexts = { "A/D 변경 · LShift 선택", "Left/Right 변경 · RShift 선택" }

local function GetPanelWidth()
    return Game.Config.bSplitScreen and 1 or 0.5
end

local function GetPanelCenterX(PlayerIndex)
    if Game.Config.bSplitScreen then
        return 0.5
    end
    return (PlayerIndex == 1) and 0.25 or 0.75
end

local function DrawCarSelectPanel(PlayerIndex)
    local CenterX = GetPanelCenterX(PlayerIndex)
    local PanelWidth = GetPanelWidth()
    local CarName = Game.SelectedCarNames and Game.SelectedCarNames[PlayerIndex] or ""
    local CarClass = VehicleCatalog[CarName] or VehicleCatalog.Default
    local bIsReady = Game.CarSelectReady and Game.CarSelectReady[PlayerIndex]

    UI.BeginPlayer(PlayerIndex)
    UI.Text(CenterX, 0.18, "P" .. PlayerIndex, 64, PlayerColors[PlayerIndex], Align.Center, "CookieRun")

    UI.Rect(CenterX - PanelWidth * 0.4, 0.70, PanelWidth * 0.8, 0.18, Color(0, 0, 0, 0.5))
    UI.Text(CenterX, 0.785, string.upper(CarName), 56, Color.White, Align.Center, "CookieRun")
    if (CarClass.Label == "스피드형") then
        UI.Text(CenterX, 0.845, CarClass.Label, 30, Color.Blue, Align.Center, "CookieRun")
    elseif (CarClass.Label == "밸런스형") then
        UI.Text(CenterX, 0.845, CarClass.Label, 30, Color.Yellow, Align.Center, "CookieRun")
    else
        UI.Text(CenterX, 0.845, CarClass.Label, 30, Color.Red, Align.Center, "CookieRun")
    end

    if bIsReady then
        UI.Text(CenterX, 0.925, "READY", 36, Color.Green, Align.Center, "CookieRun")
    else
        UI.Text(CenterX - PanelWidth * 0.32, 0.785, "<", 56, Color.Yellow, Align.Center)
        UI.Text(CenterX + PanelWidth * 0.32, 0.785, ">", 56, Color.Yellow, Align.Center)
        UI.Text(CenterX, 0.925, CarSelectHintTexts[PlayerIndex], 30, Color.White, Align.Center)
    end
    UI.EndPlayer()
end

local ControlKeyRows = {
    { Action = "전진",   P1Key = "W",      P2Key = "Up Arrow" },
    { Action = "후진",   P1Key = "S",      P2Key = "Down Arrow" },
    { Action = "좌회전", P1Key = "A",      P2Key = "Left Arrow" },
    { Action = "우회전", P1Key = "D",      P2Key = "Right Arrow" },
    { Action = "점프",   P1Key = "LCtrl",  P2Key = "RCtrl" },
    { Action = "부스트", P1Key = "LShift", P2Key = "RShift" },
}

local StateDrawHandlers = {}

local LobbyImagePath = "Assets/Image/lobbyImage.png"

StateDrawHandlers.MainMenu = function()
    UI.Image(0, 0, 1, 1, LobbyImagePath)
    UI.Rect(0.30, 0.12, 0.40, 0.76, Color(0, 0, 0, 0.45))
    UI.Text(0.5, 0.25, "Jungle League", 96, Color.White, Align.Center, "CookieRun")
    DrawMenuItems(0.45)
    DrawBottomHint("W/S  UpArrow/DownArrow 이동 · Enter 선택")
end

StateDrawHandlers.Controls = function()
    UI.Image(0, 0, 1, 1, LobbyImagePath)
    DrawBackground()
    UI.Text(0.5, 0.18, "조작키", 64, Color.White, Align.Center)
    UI.Text(0.45, 0.28, "P1", 36, Color.Orange, Align.Center)
    UI.Text(0.65, 0.28, "P2", 36, Color.Blue, Align.Center)
    for RowIndex, KeyRow in ipairs(ControlKeyRows) do
        local RowY = 0.28 + RowIndex * 0.07
        UI.Text(0.30, RowY, KeyRow.Action, 32, Color.White, Align.Center)
        UI.Text(0.45, RowY, KeyRow.P1Key, 32, Color.White, Align.Center)
        UI.Text(0.65, RowY, KeyRow.P2Key, 32, Color.White, Align.Center)
    end
    DrawBottomHint(BackHintText)
end

StateDrawHandlers.Credit = function()
    UI.Image(0, 0, 1, 1, LobbyImagePath)
    DrawBackground()
    UI.Text(0.50, 0.28, "팀원", 40, Color.White, Align.Center)
    for CreditIndex, Credit in ipairs(Game.Config.Credits) do
        local RowY = 0.28 + CreditIndex * 0.10
        UI.Text(0.50, RowY, Credit.Name, 50, Color.White, Align.Center)
    end
    DrawBottomHint(BackHintText)
end

StateDrawHandlers.CarSelect = function()
    UI.Text(0.5, 0.07, "SELECT YOUR CAR", 48, Color.White, Align.Center, "CookieRun")
    DrawCarSelectPanel(1)
    DrawCarSelectPanel(2)
    UI.Text(0.5, 0.985, BackHintText, 24, Color.White, Align.Center)
end

StateDrawHandlers.Countdown = function()
    DrawHUD()
    DrawTurnBanner()
    DrawCountdown()
end

StateDrawHandlers.Playing = function()
    DrawHUD()
    DrawCountdown()
end

StateDrawHandlers.GoalScored = function()
    DrawHUD()
    UI.Text(0.5, 0.45, "GOAL!", 140, Color.Yellow, Align.Center, "CookieRun")
    UI.Text(0.5, 0.55, string.format("P%d  %d : %d", Game.LastScorerIndex or 0, Game.Score[1], Game.Score[2]),
        48, Color.White, Align.Center)
end

StateDrawHandlers.Defended = function()
    DrawHUD()
    UI.Text(0.5, 0.45, "DEFENDED!", 120, Color.Blue, Align.Center, "CookieRun")
end

StateDrawHandlers.Result = function()
    DrawBackground()

    local WinnerColor = (Game.WinnerIndex == 1) and Color.Orange or Color.Blue
    UI.Text(0.5, 0.25, string.format("P%d WIN", Game.WinnerIndex or 0), 140, WinnerColor, Align.Center, "CookieRun")
    UI.Text(0.5, 0.36, string.format("%d : %d", Game.Score[1], Game.Score[2]), 64, Color.White, Align.Center)
    if Game.bIsSuddenDeath then
        UI.Text(0.5, 0.42, "SUDDEN DEATH", 28, Color.Red, Align.Center)
    end

    DrawMenuItems(0.58)
    DrawBottomHint("W/S  UpArrow/DownArrow 이동 · Enter 선택")
end

function Tick(DeltaTime)
    if not Game.State or not Game.Config or not Game.Menu then return end

    local StateDrawHandler = StateDrawHandlers[Game.State]
    if StateDrawHandler then
        StateDrawHandler()
    end
end
