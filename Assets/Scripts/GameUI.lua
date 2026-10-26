local HintTextColor = Color(0.8, 0.8, 0.8)
local BackHintText = "Backspace 뒤로"

local function DrawBackground()
    UI.Rect(0, 0, 1, 1, Color(0, 0, 0, 0.55))
end

local function DrawBottomHint(HintText)
    UI.Text(0.5, 0.93, HintText, 24, HintTextColor, Align.Center)
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

local function DrawDebugOverlay()
    if not Game.bShowDebug then return end

    UI.Rect(0.72, 0.01, 0.27, 0.16, Color(0, 0, 0, 0.6))
    UI.Text(0.73, 0.045, "State: " .. tostring(Game.State), 24, Color.Yellow)
    UI.Text(0.73, 0.08, string.format("Turn %d  Attacker %s  SD %s",
        Game.Turn or 0, tostring(Game.AttackerIndex), tostring(Game.bIsSuddenDeath)), 20)
    UI.Text(0.73, 0.11, string.format("Score %d : %d  Time %.1f",
        Game.Score[1], Game.Score[2], Game.RemainTime or 0), 20)

    local bUsingStub = (Game.Cars and Game.Cars[1] and Game.Cars[1].bIsStub) or (Game.Ball and Game.Ball.bIsStub)
    UI.Text(0.73, 0.14, "Cars/Ball: " .. (bUsingStub and "STUB" or "REAL"), 20, Color(0.7, 0.7, 0.7))
end

local StateDrawHandlers = {}

StateDrawHandlers.MainMenu = function()
    DrawBackground()
    UI.Text(0.5, 0.25, "ROCKET HALF", 96, Color.White, Align.Center, "CookieRun")
    DrawMenuItems(0.45)
    DrawBottomHint("W/S  UpArrow/DownArrow 이동 · Enter 선택")
end

local ControlKeyRows = {
    { Action = "전진",   P1Key = "W",      P2Key = "Up Arrow" },
    { Action = "후진",   P1Key = "S",      P2Key = "Down Arrow" },
    { Action = "좌회전", P1Key = "A",      P2Key = "Left Arrow" },
    { Action = "우회전", P1Key = "D",      P2Key = "Right Arrow" },
    { Action = "점프",   P1Key = "LCtrl",  P2Key = "RCtrl" },
    { Action = "부스트", P1Key = "LShift", P2Key = "RShift" },
}

StateDrawHandlers.Controls = function()
    DrawBackground()
    UI.Text(0.5, 0.18, "조작키", 64, Color.White, Align.Center)
    UI.Text(0.45, 0.28, "P1", 36, Color.Blue, Align.Center)
    UI.Text(0.65, 0.28, "P2", 36, Color.Orange, Align.Center)
    for RowIndex, KeyRow in ipairs(ControlKeyRows) do
        local RowY = 0.28 + RowIndex * 0.07
        UI.Text(0.30, RowY, KeyRow.Action, 32, Color.White, Align.Center)
        UI.Text(0.45, RowY, KeyRow.P1Key, 32, Color.White, Align.Center)
        UI.Text(0.65, RowY, KeyRow.P2Key, 32, Color.White, Align.Center)
    end
    DrawBottomHint(BackHintText)
end

StateDrawHandlers.Credit = function()
    DrawBackground()
    UI.Text(0.50, 0.28, "팀원", 40, Color.White, Align.Center)
    for CreditIndex, CreditName in ipairs (Game.Config.Credits) do
        local RowY = 0.28 + CreditIndex * 0.07
        UI.Text(0.50, RowY, CreditName.Name, 32, Color.White, Align.Center)
    end
    DrawBottomHint(BackHintText)
end

StateDrawHandlers.Option = function()
    DrawBackground()
    UI.Text(0.5, 0.45, "준비 중", 56, Color.White, Align.Center)
    DrawBottomHint(BackHintText)
end

function Tick(DeltaTime)
    if not Game.State or not Game.Config or not Game.Menu then return end

    local StateDrawHandler = StateDrawHandlers[Game.State]
    if StateDrawHandler then
        StateDrawHandler()
    end

    DrawDebugOverlay()
end
