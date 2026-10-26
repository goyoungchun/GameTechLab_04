
_G.KeyBindings = _G.KeyBindings or {
	[0] = { Forward = Key.W, Backward = Key.S, Left = Key.A, Right = Key.D, Boost = Key.LShift, Jump = Key.LControl },
	[1] = { Forward = Key.Up, Backward = Key.Down, Left = Key.Left, Right = Key.Right, Boost = Key.RShift, Jump = Key.RControl },
}

_G.CarPresets = {
	Default = {
		-- 질량과 속도·가속도는 공통 VehicleCatalog의 분류 설정을 사용한다.
		Deceleration = 30,		-- 키를 뗌, 반대 키, 부스트 끝
		Grip = 10,				-- 클수록 레일처럼 붙고, 작을수록 미끄러진다
		TurnRate = 100,			-- 도/초
		FullTurnSpeed = 7.5,		-- 이 속력부터 TurnRate로 돈다
		JumpSpeed = 6,			-- 높이 ≈ JumpSpeed² / 19.6
	},

	-- 균형형
	coupe = {},
	police = { Grip = 11, TurnRate = 105 },

	-- 스피드형: 빠르지만 덜 붙고 일부 모델은 점프가 낮다
	italia = { Grip = 8.5, TurnRate = 105 },
	lamb = { Grip = 8, JumpSpeed = 5.2 },
	ghini = { Grip = 9, TurnRate = 110, JumpSpeed = 5.5 },
	fenyr = { Grip = 8.5, TurnRate = 90 },

	-- 스피드형: 낮은 접지력으로 미끄러짐이 크다
	kamaro = { Grip = 7, TurnRate = 95 },

	-- 기동형: 잘 돌고 잘 붙는다
	rally = { Grip = 13, TurnRate = 120, FullTurnSpeed = 6, JumpSpeed = 6.5 },

	-- 중량형 jeep / 밸런스형 mobil: 높은 점프
	jeep = { Grip = 12, TurnRate = 95, JumpSpeed = 7.5 },
	mobil = { TurnRate = 90, JumpSpeed = 6.5 },

	-- 중량형: 느리고 둔하지만 단단히 붙는다
	armor = { Deceleration = 36, Grip = 13, TurnRate = 85, JumpSpeed = 5.5 },
	van = { Grip = 11, TurnRate = 85, JumpSpeed = 6.5 },
}

local Car = obj:AsVehicle()
local AppliedBody = nil

local function Axis(PositiveKey, NegativeKey)
	local Value = 0
	if IsKeyDown(PositiveKey) then Value = Value + 1 end
	if IsKeyDown(NegativeKey) then Value = Value - 1 end
	return Value
end

-- 공통 분류의 속도/가속도에 기본 조작 값과 차종별 특성을 더한다.
local function BuildPreset(BodyName)
	local Result = {}
	local Class = VehicleCatalog[BodyName] or VehicleCatalog.Default
	for Name, Value in pairs(Class.Tuning) do Result[Name] = Value end
	for Name, Value in pairs(CarPresets.Default) do Result[Name] = Value end
	for Name, Value in pairs(CarPresets[BodyName] or {}) do Result[Name] = Value end
	return Result
end

local function ApplyPreset()
	AppliedBody = Car.BodyName
	Car:SetTuning(BuildPreset(AppliedBody))

	local Note = CarPresets[AppliedBody] and "" or " (no preset, Default only)"
	print(string.format("[Vehicle] P%d preset: %s%s", Car.PlayerIndex, AppliedBody, Note))
end

local function SetCarModel(CarName)
	if not Car:SetBody(CarName) then
		LogWarning("[Vehicle] Unknown car model: " .. tostring(CarName))
	end
end

local function BindCarModel()
	local Slot = Game.Cars and Game.Cars[Car.PlayerIndex + 1]
	if Slot and Slot.SetCarModel ~= SetCarModel then
		Slot.SetCarModel = SetCarModel
	end
end

-- 처음 로드할 때와 핫 리로드할 때마다 다시 적용한다. (BeginPlay는 핫 리로드 때 다시 불리지 않는다)
if Car then ApplyPreset() end

function BeginPlay()
	if not Car then
		LogError("VehicleController.lua: not attached to a vehicle - " .. obj.Name)
		return
	end
	obj:AddAsPlayer()
	print(string.format("[Vehicle] %s PlayerIndex = %d", obj.Name, Car.PlayerIndex))

	Game.Vehicles = Game.Vehicles or {}
	Game.Vehicles[Car.PlayerIndex + 1] = obj

end

function EndPlay()
print("[EndPlay] " .. obj.UUID)
end

function Tick(dt)

	if not Car then return end

	BindCarModel()

	-- 누가 SetBody로 차체를 바꿨든(차량 선택 등) 그 차체의 프리셋을 맞춘다.
	if Car.BodyName ~= AppliedBody then ApplyPreset() end

	local Keys = KeyBindings[Car.PlayerIndex]
	if not Keys then return end

	local InputEnabled = Game.CarInputEnabled and Game.CarInputEnabled[Car.PlayerIndex + 1]
	if InputEnabled == false then
		Car:SetInput(0, 0, false)
		return
	end

	Car:SetInput(Axis(Keys.Forward, Keys.Backward), Axis(Keys.Right, Keys.Left), IsKeyDown(Keys.Boost))

	if IsKeyPressed(Keys.Jump) then
		Car:Jump()
	end
end
