Game.Config = {
    TurnTime       = 60,
    TotalTurns     = 4,
    CountdownTime  = 3,
    ResultShowTime = 2,
    BoostSeconds   = 3,
    CarNames = { "armor", "coupe", "fenyr", "ghini", "italia", "jeep", "kamaro", "lamb", "mobil", "police" },
    BackKey = Key.Backspace,
    MainMenuItems = { "Play", "Help", "Credit", "Exit" },
    Credits = {
        { Name = "김현준" },
        { Name = "이민규" },
        { Name = "정상윤" },
        { Name = "황혜진" },
    },
}

Game.Menu = Game.Menu or {
    Items = {},
    SelectedIndex = 1,
}

if Game.bShowDebug == nil then
    Game.bShowDebug = true
end

local function ResetMatchState()
    Game.Turn = 0
    Game.bIsSuddenDeath = false
    Game.AttackerIndex = nil
    Game.Score = { 0, 0 }
    Game.RemainTime = 0
    Game.Countdown = nil
    Game.WinnerIndex = nil
    Game.SelectedCarNames = { nil, nil }
end

local function EnsureStubCarsAndBall()
    Game.Cars = Game.Cars or {}
    for PlayerIndex = 1, 2 do
        if not Game.Cars[PlayerIndex] then
            local BoostRatio = 1
            Game.Cars[PlayerIndex] = {
                bIsStub          = true,
                SetInputEnabled  = function(bEnabled) print("[Stub] Car" .. PlayerIndex .. " input", bEnabled) end,
                RefillBoost      = function() BoostRatio = 1 end,
                SetInfiniteBoost = function(bInfinite) end,
                GetBoostRatio    = function() return BoostRatio end,
                ResetTo          = function(Location, Yaw) print("[Stub] Car" .. PlayerIndex .. " reset") end,
                SetCarModel      = function(CarName) print("[Stub] Car" .. PlayerIndex .. " model", CarName) end,
            }
        end
    end
    Game.Ball = Game.Ball or {
        bIsStub = true,
        ResetTo = function(Location) print("[Stub] Ball reset") end,
    }
end

local function SetAllCarsInputEnabled(bEnabled)
    for PlayerIndex = 1, 2 do
        Game.Cars[PlayerIndex].SetInputEnabled(bEnabled)
    end
end

local function SetMenuItems(MenuItems)
    Game.Menu.Items = MenuItems
    Game.Menu.SelectedIndex = 1
end

local function UpdateMenuSelection()
    local MenuState = Game.Menu
    if IsKeyPressed(Key.W) or IsKeyPressed(Key.Up) then
        MenuState.SelectedIndex = math.max(1, MenuState.SelectedIndex - 1)
    end
    if IsKeyPressed(Key.S) or IsKeyPressed(Key.Down) then
        MenuState.SelectedIndex = math.min(#MenuState.Items, MenuState.SelectedIndex + 1)
    end
    if IsKeyPressed(Key.Enter) then
        return MenuState.Items[MenuState.SelectedIndex]
    end
    return nil
end

local StateEnterHandlers = {}
local StateTickHandlers = {}

local function SetState(NewState)
    print(string.format("[GameMode] %s -> %s", tostring(Game.State), NewState))
    Game.State = NewState
    local EnterHandler = StateEnterHandlers[NewState]
    if EnterHandler then
        EnterHandler()
    end
end
Game.SetState = SetState

local function HandleBackToMainMenu()
    if IsKeyPressed(Game.Config.BackKey) then
        SetState("MainMenu")
    end
end

local MainMenuElapsedTime = 0

StateEnterHandlers.MainMenu = function()
    SetMenuItems(Game.Config.MainMenuItems)
    ResetMatchState()
    SetAllCarsInputEnabled(false)
    MainMenuElapsedTime = 0
end

StateTickHandlers.MainMenu = function(DeltaTime)
    MainMenuElapsedTime = MainMenuElapsedTime + DeltaTime

    local ChosenItem = UpdateMenuSelection()
    if ChosenItem == "Play" then
        local RandomSeed = math.floor(MainMenuElapsedTime * 1000)
        math.randomseed(RandomSeed)
        print("[GameMode] Random seed", RandomSeed)
        SetState("CarSelect")
    elseif ChosenItem == "Help" then
        SetState("Controls")
    elseif ChosenItem == "Credit" then
        SetState("Credit")
    elseif ChosenItem == "Exit" then
        print("[GameMode] Quit requested (QuitGame 미구현)")
    end
end

StateTickHandlers.Controls = function(DeltaTime) HandleBackToMainMenu() end
StateTickHandlers.Credit   = function(DeltaTime) HandleBackToMainMenu() end
StateTickHandlers.Option   = function(DeltaTime) HandleBackToMainMenu() end

StateTickHandlers.CarSelect  = function(DeltaTime) end
StateTickHandlers.Countdown  = function(DeltaTime) end
StateTickHandlers.Playing    = function(DeltaTime) end
StateTickHandlers.GoalScored = function(DeltaTime) end
StateTickHandlers.Defended   = function(DeltaTime) end
StateTickHandlers.Result     = function(DeltaTime) end

local DebugStateNames = { "MainMenu", "CarSelect", "Countdown", "Playing", "Result" }

local function HandleDebugKeys()
    if IsKeyPressed(Key.Tab) then
        Game.bShowDebug = not Game.bShowDebug
    end
    for StateIndex, StateName in ipairs(DebugStateNames) do
        if IsKeyPressed(Key["Num" .. StateIndex]) then
            SetState(StateName)
        end
    end
end

function BeginPlay()
    ResetMatchState()
    EnsureStubCarsAndBall()
    SetState("MainMenu")
end

function Tick(DeltaTime)
    EnsureStubCarsAndBall()

    local StateTickHandler = StateTickHandlers[Game.State]
    if StateTickHandler then
        StateTickHandler(DeltaTime)
    else
        LogWarning("[GameMode] Unknown state: " .. tostring(Game.State))
    end

    HandleDebugKeys()
end
