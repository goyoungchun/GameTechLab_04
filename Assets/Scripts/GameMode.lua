-- 상태: Ready → Playing → GameOver → (Enter) → Ready

local TIME_LIMIT = 30
local RemainTime = 0

local function UpdateText()
    if Game.State == "Ready" then
        obj:SetText("Press Enter to Start")
    elseif Game.State == "Playing" then
        obj:SetText(string.format("Score: %d   Time: %d", Game.Score, math.ceil(RemainTime)))
    elseif Game.State == "GameOver" then
        obj:SetText(string.format("Game Over   Score: %d   Press Enter", Game.Score))
    end
end

function Game.StartGame()
    Game.Score = 0
    RemainTime = TIME_LIMIT
    Game.State = "Playing"
    UpdateText()
end

function Game.EndGame()
    if Game.State ~= "Playing" then return end
    Game.State = "GameOver"
    UpdateText()
end

function Game.AddScore(Amount)
    if Game.State ~= "Playing" then return end
    Game.Score = Game.Score + Amount
    UpdateText()
end

local function Restart()
    if RestartLevel then
        RestartLevel()
    else
        Game.State = "Ready"
        Game.Score = 0
        UpdateText()
    end
end

function BeginPlay()
    Game.State = "Ready"
    Game.Score = 0
    UpdateText()
end

function Tick(dt)
    if Game.State == "Ready" then
        if IsKeyPressed(Key.Enter) then Game.StartGame() end

    elseif Game.State == "Playing" then
        RemainTime = RemainTime - dt
        if RemainTime <= 0 then
            RemainTime = 0
            Game.EndGame()
            return
        end

        if IsKeyPressed(Key.Up) then Game.AddScore(1) end

        UpdateText()

    elseif Game.State == "GameOver" then
        if IsKeyPressed(Key.Enter) then Restart() end
    end
end