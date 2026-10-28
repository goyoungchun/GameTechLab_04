
_G.KeyBindings = _G.KeyBindings or {
	[0] = { Forward = Key.W, Backward = Key.S, Left = Key.A, Right = Key.D, Boost = Key.LShift, Jump = Key.LControl },
	[1] = { Forward = Key.Up, Backward = Key.Down, Left = Key.Left, Right = Key.Right, Boost = Key.RShift, Jump = Key.RControl },
}

_G.CarPresets = {
	Default = {
		-- 질량과 속도·가속도는 공통 VehicleCatalog의 분류 설정을 사용한다.
		-- 전후진 모두 A/D 입력 방향으로 회전하고, 고속에서는 조향을 완화한다.
		Grip = 10,
		TurnRate = 120,
		SteeringSpeedScale = 100,
		BoostMultiplier = 2,
		JumpSpeed = 18,			-- 높이 ≈ JumpSpeed² / (19.6 * GravityScale)
	},

	-- 균형형
	coupe = {},
	police = {},

	-- 스피드형
	italia = { JumpSpeed = 19 },
	lamb = { JumpSpeed = 19.2 },
	ghini = { JumpSpeed = 19.5 },
	fenyr = { JumpSpeed = 19 },

	kamaro = { JumpSpeed = 19 },

	rally = { JumpSpeed = 25.5 },

	-- 중량형 jeep / 밸런스형 mobil: 높은 점프
	jeep = { JumpSpeed = 18.5 },
	mobil = { JumpSpeed = 15.5 },

	armor = { JumpSpeed = 18.5 },
	van = { JumpSpeed = 18.5 },
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
	for Name, Value in pairs(CarPresets.Default) do Result[Name] = Value end
	for Name, Value in pairs(Class.Tuning) do Result[Name] = Value end
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
