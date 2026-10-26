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
    StartSpots = {
        Attacker     = { Location = Vector(0, 2, 0),  Yaw = 90 },
        Defender     = { Location = Vector(0, 36, 0), Yaw = -90 },
        SuddenDeath1 = { Location = Vector(-8, 2, 0), Yaw = 90 },
        SuddenDeath2 = { Location = Vector(8, 2, 0),  Yaw = 90 },
        Ball         = Vector(0, 14, 2),
    },
    DefaultCarNames = { "coupe", "jeep" },
    GoTextTime = 0.7,
    ResultMenuItems = { "Restart", "Main Menu", "Exit" },
    ResultInputDelay = 1.0,
    bSplitScreen = false,
    CarSelectKeys = {
        { Prev = Key.A,    Next = Key.D,     Confirm = Key.LShift },
        { Prev = Key.Left, Next = Key.Right, Confirm = Key.RShift },
    },
    Showroom = {
        -- 차량 중심이 아니라 받침대 윗면의 위치. 모델별 하단 높이는 배치할 때 계산한다.
        CarLocations = { Vector(-48, -8, 0.6), Vector(-48, 8, 0.6) },
        CameraSpots = {
            { Location = Vector(-60, -8, 3.5), Rotation = Rotator(10, 0, 0) },
            { Location = Vector(-60, 8, 3.5),  Rotation = Rotator(10, 0, 0) },
        },
        SharedCameraSpot = { Location = Vector(-66, 0, 5), Rotation = Rotator(12, 0, 0) },
        HiddenLocation = Vector(-48, 0, -200),
        TurnSpeed = 30,
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
    Game.FirstAttackerIndex = nil
    Game.LastScorerIndex = nil
end

local function SpawnTurnBall(Location)
    return obj:SpawnBall(Location)
end

local function ClearTurnBall()
    obj:ClearBalls()
end

local function EnsureCarsAndBall()
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
    -- 공의 네이티브 포인터를 보관하지 않고, 현재 World에서 새 공 하나로 교체한다.
    Game.Ball = Game.Ball or {}
    Game.Ball.bIsStub = false
    Game.Ball.ResetTo = SpawnTurnBall
    Game.Ball.Clear = ClearTurnBall
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

local SplitScreenStates = 
{
    Countdown = true, Playing = true, GoalScored =true , Defended = true,
}

local function SetState(NewState)
    print(string.format("[GameMode] %s -> %s", tostring(Game.State), NewState))
    Game.State = NewState
    obj:SetSplitScreen(SplitScreenStates[NewState] == true 
        or (NewState == "CarSelect" and Game.Config.bSplitScreen))

    local EnterHandler = StateEnterHandlers[NewState]
    if EnterHandler then
        EnterHandler()
    end
end
Game.SetState = SetState

local function RequestQuitGame()
    print("[GameMode] Quit requested")
    QuitGame()
end

local function HandleBackToMainMenu()
    if IsKeyPressed(Game.Config.BackKey) then
        SetState("MainMenu")
    end
end

local function GetDefenderIndex(AttackerIndex)
    return 3 - AttackerIndex
end

local function GetAttackerForTurn(Turn)
    if Turn % 2 == 1 then
        return Game.FirstAttackerIndex
    end
    return GetDefenderIndex(Game.FirstAttackerIndex)
end

local CountdownCoroutineId = nil
local ResultCoroutineId = nil

local function StopCountdown()
    if CountdownCoroutineId then
        StopCoroutine(CountdownCoroutineId)
        CountdownCoroutineId = nil
    end
    Game.Countdown = nil
end

local function StopTurnResult()
    if ResultCoroutineId then
        StopCoroutine(ResultCoroutineId)
        ResultCoroutineId = nil
    end
end

local function StartTurn()
    local Spots = Game.Config.StartSpots

    if Game.bIsSuddenDeath then
        Game.AttackerIndex = nil
        Game.Cars[1].ResetTo(Spots.SuddenDeath1.Location, Spots.SuddenDeath1.Yaw)
        Game.Cars[2].ResetTo(Spots.SuddenDeath2.Location, Spots.SuddenDeath2.Yaw)
        for PlayerIndex = 1, 2 do
            Game.Cars[PlayerIndex].SetInfiniteBoost(true)
        end
    else
        local AttackerIndex = GetAttackerForTurn(Game.Turn)
        local DefenderIndex = GetDefenderIndex(AttackerIndex)
        Game.AttackerIndex = AttackerIndex

        Game.Cars[AttackerIndex].ResetTo(Spots.Attacker.Location, Spots.Attacker.Yaw)
        Game.Cars[DefenderIndex].ResetTo(Spots.Defender.Location, Spots.Defender.Yaw)
        for PlayerIndex = 1, 2 do
            Game.Cars[PlayerIndex].RefillBoost()
            Game.Cars[PlayerIndex].SetInfiniteBoost(false)
        end
    end

    Game.Ball.ResetTo(Spots.Ball)
    Game.RemainTime = Game.Config.TurnTime
    Game.LastTouchIndex = nil
    SetState("Countdown")
end

local function CountRemainingAttacks(PlayerIndex)
    local RemainingAttacks = 0
    for FutureTurn = Game.Turn + 1, Game.Config.TotalTurns do
        if GetAttackerForTurn(FutureTurn) == PlayerIndex then
            RemainingAttacks = RemainingAttacks + 1
        end
    end
    return RemainingAttacks
end

local function FindDecidedWinner()
    for PlayerIndex = 1, 2 do
        local OpponentIndex = GetDefenderIndex(PlayerIndex)
        if Game.Score[PlayerIndex] > Game.Score[OpponentIndex] + CountRemainingAttacks(OpponentIndex) then
            return PlayerIndex
        end
    end
    return nil
end

local function FinishMatch(WinnerIndex)
    Game.WinnerIndex = WinnerIndex
    print(string.format("[GameMode] Match end. Winner: P%d  %d : %d", WinnerIndex, Game.Score[1], Game.Score[2]))
    SetState("Result")
end

local function StartSuddenDeath()
    print("[GameMode] Sudden death")
    Game.bIsSuddenDeath = true
    StartTurn()
end

local function EndTurn()
    if Game.bIsSuddenDeath then
        FinishMatch(Game.LastScorerIndex)
        return
    end

    local WinnerIndex = FindDecidedWinner()
    if WinnerIndex then
        FinishMatch(WinnerIndex)
        return
    end

    if Game.Turn >= Game.Config.TotalTurns then
        StartSuddenDeath()
        return
    end

    Game.Turn = Game.Turn + 1
    StartTurn()
end

local function StartMatch()
    ResetMatchState()
    Game.FirstAttackerIndex = math.random(1, 2)
    for PlayerIndex = 1, 2 do
        Game.Cars[PlayerIndex].SetCarModel(Game.SelectedCarNames[PlayerIndex])
    end
    print("[GameMode] Match start. First attacker: P" .. Game.FirstAttackerIndex)
    Game.Turn = 1
    StartTurn()
end

local function CountdownRoutine()
    SetAllCarsInputEnabled(false)
    for Count = Game.Config.CountdownTime, 1, -1 do
        Game.Countdown = Count
        coroutine.yield(WaitForSeconds(1))
    end

    Game.Countdown = 0
    SetState("Playing")
    SetAllCarsInputEnabled(true)

    coroutine.yield(WaitForSeconds(Game.Config.GoTextTime))
    Game.Countdown = nil
    CountdownCoroutineId = nil
end

local function ShowTurnResultThenEnd()
    SetAllCarsInputEnabled(false)
    coroutine.yield(WaitForSeconds(Game.Config.ResultShowTime))
    ResultCoroutineId = nil
    EndTurn()
end

local ShowroomYaw = 0

local function FindCarIndex(CarName)
    for CarIndex, Name in ipairs(Game.Config.CarNames) do
        if Name == CarName then
            return CarIndex
        end
    end
    return 1
end

local function GetShowroomCar(PlayerIndex)
    Game.ShowroomCars = Game.ShowroomCars or {}
    local Car = Game.ShowroomCars[PlayerIndex]
    if Car and not Car:IsActorBeingDestroyed() then
        return Car
    end
    Car = obj:SpawnActor("", Game.Config.Showroom.CarLocations[PlayerIndex], Game.SelectedCarNames[PlayerIndex])
    Game.ShowroomCars[PlayerIndex] = Car
    return Car
end

local function SetShowroomCameras()
    local Showroom = Game.Config.Showroom
    if Game.Config.bSplitScreen then
        for PlayerIndex = 1, 2 do
            local CameraSpot = Showroom.CameraSpots[PlayerIndex]
            obj:SetViewCamera(PlayerIndex, CameraSpot.Location, CameraSpot.Rotation)
        end
    else
        obj:SetViewCamera(1, Showroom.SharedCameraSpot.Location, Showroom.SharedCameraSpot.Rotation)
    end
end

local function EnterShowroom()
    for PlayerIndex = 1, 2 do
        local Car = GetShowroomCar(PlayerIndex)
        if Car then
            Car:SetMesh(Game.SelectedCarNames[PlayerIndex])
            Car:PlaceOnSurface(Game.Config.Showroom.CarLocations[PlayerIndex])
        end
    end
    SetShowroomCameras()
end

local function LeaveShowroom()
    for PlayerIndex = 1, 2 do
        local Car = Game.ShowroomCars and Game.ShowroomCars[PlayerIndex]
        if Car then
            Car.Location = Game.Config.Showroom.HiddenLocation
        end
        obj:ClearViewCamera(PlayerIndex)
    end
end

local function TurnShowroomCars(DeltaTime)
    ShowroomYaw = (ShowroomYaw + Game.Config.Showroom.TurnSpeed * DeltaTime) % 360
    for PlayerIndex = 1, 2 do
        local Car = Game.ShowroomCars and Game.ShowroomCars[PlayerIndex]
        if Car then
            Car.Rotation = Rotator(0, ShowroomYaw, 0)
        end
    end
end

local function ChangeCar(PlayerIndex, Direction)
    local CarNames = Game.Config.CarNames
    local OtherCarName = Game.SelectedCarNames[GetDefenderIndex(PlayerIndex)]
    local CarIndex = FindCarIndex(Game.SelectedCarNames[PlayerIndex])
    repeat
        CarIndex = (CarIndex - 1 + Direction) % #CarNames + 1
    until CarNames[CarIndex] ~= OtherCarName

    Game.SelectedCarNames[PlayerIndex] = CarNames[CarIndex]
    local Car = Game.ShowroomCars and Game.ShowroomCars[PlayerIndex]
    if Car then
        Car:SetMesh(CarNames[CarIndex])
        Car:PlaceOnSurface(Game.Config.Showroom.CarLocations[PlayerIndex])
    end
end

local MainMenuElapsedTime = 0

StateEnterHandlers.MainMenu = function()
    StopCountdown()
    StopTurnResult()
    LeaveShowroom()
    Game.Ball.Clear()
    SetMenuItems(Game.Config.MainMenuItems)
    ResetMatchState()
    Game.SelectedCarNames = { nil, nil }
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
        RequestQuitGame()
    end
end

StateTickHandlers.Controls = function(DeltaTime) HandleBackToMainMenu() end
StateTickHandlers.Credit   = function(DeltaTime) HandleBackToMainMenu() end

StateEnterHandlers.CarSelect = function()
    Game.Ball.Clear()
    local PreviousCarNames = Game.SelectedCarNames or {}
    Game.SelectedCarNames = {
        PreviousCarNames[1] or Game.Config.DefaultCarNames[1],
        PreviousCarNames[2] or Game.Config.DefaultCarNames[2],
    }
    Game.CarSelectReady = { false, false }
    SetAllCarsInputEnabled(false)
    EnterShowroom()
end

StateTickHandlers.CarSelect = function(DeltaTime)
    if IsKeyPressed(Game.Config.BackKey) then
        SetState("MainMenu")
        return
    end

    for PlayerIndex = 1, 2 do
        local Keys = Game.Config.CarSelectKeys[PlayerIndex]
        if IsKeyPressed(Keys.Confirm) then
            Game.CarSelectReady[PlayerIndex] = not Game.CarSelectReady[PlayerIndex]
        elseif not Game.CarSelectReady[PlayerIndex] then
            if IsKeyPressed(Keys.Prev) then
                ChangeCar(PlayerIndex, -1)
            elseif IsKeyPressed(Keys.Next) then
                ChangeCar(PlayerIndex, 1)
            end
        end
    end

    TurnShowroomCars(DeltaTime)

    if Game.CarSelectReady[1] and Game.CarSelectReady[2] then
        LeaveShowroom()
        StartMatch()
    end
end

StateEnterHandlers.Countdown = function()
    StopCountdown()
    CountdownCoroutineId = StartCoroutine(CountdownRoutine)
end

StateTickHandlers.Countdown = function(DeltaTime) end

StateTickHandlers.Playing = function(DeltaTime)
    if Game.bIsSuddenDeath then return end
    Game.RemainTime = math.max(0, Game.RemainTime - DeltaTime)
    if Game.RemainTime <= 0 then
        SetState("Defended")
    end
end

StateEnterHandlers.GoalScored = function()
    StopTurnResult()
    ResultCoroutineId = StartCoroutine(ShowTurnResultThenEnd)
end

StateTickHandlers.GoalScored = function(DeltaTime) end

StateEnterHandlers.Defended = function()
    print("[GameMode] Defended by P" .. GetDefenderIndex(Game.AttackerIndex))
    StopTurnResult()
    ResultCoroutineId = StartCoroutine(ShowTurnResultThenEnd)
end

StateTickHandlers.Defended = function(DeltaTime) end

Game.OnGoal = function()
    if Game.State ~= "Playing" then return end

    local ScorerIndex = Game.AttackerIndex
    if Game.bIsSuddenDeath then
        ScorerIndex = Game.LastTouchIndex
        if not ScorerIndex then
            Game.Ball.ResetTo(Game.Config.StartSpots.Ball)
            return
        end
    end

    Game.Score[ScorerIndex] = Game.Score[ScorerIndex] + 1
    Game.LastScorerIndex = ScorerIndex
    print(string.format("[GameMode] GOAL! P%d  %d : %d", ScorerIndex, Game.Score[1], Game.Score[2]))
    SetState("GoalScored")
end

local ResultElapsedTime = 0

StateEnterHandlers.Result = function()
    SetAllCarsInputEnabled(false)
    SetMenuItems(Game.Config.ResultMenuItems)
    ResultElapsedTime = 0
end

StateTickHandlers.Result = function(DeltaTime)
    ResultElapsedTime = ResultElapsedTime + DeltaTime
    if ResultElapsedTime < Game.Config.ResultInputDelay then return end

    local ChosenItem = UpdateMenuSelection()
    if ChosenItem == "Restart" then
        SetState("CarSelect")
    elseif ChosenItem == "Main Menu" then
        SetState("MainMenu")
    elseif ChosenItem == "Exit" then
        RequestQuitGame()
    end
end

local DebugKeyActions = {
    [1] = function() SetState("MainMenu") end,
    [2] = function() SetState("CarSelect") end,
    [3] = function()
        if not Game.FirstAttackerIndex then return end
        EndTurn()
    end,
    [4] = function() Game.LastTouchIndex = Game.AttackerIndex or 1; Game.OnGoal() end,
    [5] = function() Game.RemainTime = 0 end,
    [6] = function() Game.LastTouchIndex = 2; Game.OnGoal() end,
}

local function HandleDebugKeys()
    if IsKeyPressed(Key.Tab) then
        Game.bShowDebug = not Game.bShowDebug
    end
    for KeyNumber, Action in pairs(DebugKeyActions) do
        if IsKeyPressed(Key["Num" .. KeyNumber]) then
            Action()
        end
    end
end

function BeginPlay()
    ResetMatchState()
    EnsureCarsAndBall()
    SetState("MainMenu")
end

function Tick(DeltaTime)
    EnsureCarsAndBall()

    local StateTickHandler = StateTickHandlers[Game.State]
    if StateTickHandler then
        StateTickHandler(DeltaTime)
    else
        LogWarning("[GameMode] Unknown state: " .. tostring(Game.State))
    end

    HandleDebugKeys()
end
