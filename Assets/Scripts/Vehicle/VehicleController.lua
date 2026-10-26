_G.KeyBindings = _G.KeyBindings or {
	[0] = { Forward = Key.W, Backward = Key.S, Left = Key.A, Right = Key.D, Boost = Key.LShift, Jump = Key.LControl },
	[1] = { Forward = Key.Up, Backward = Key.Down, Left = Key.Left, Right = Key.Right, Boost = Key.RShift, Jump = Key.RControl },
}

_G.CarPresets = {
	Default = {
		MaxForwardSpeed = 20,	-- u/s
		MaxReverseSpeed = 8,
		BoostMaxSpeed = 30,		-- MaxForwardSpeed 이상, 100 미만
		Acceleration = 15,		-- 속력을 높일 때 (u/s²)
		Deceleration = 20,		-- 키를 뗌, 반대 키, 부스트 끝
		Grip = 10,				-- 클수록 레일처럼 붙고, 작을수록 미끄러진다
		TurnRate = 100,			-- 도/초
		FullTurnSpeed = 5,		-- 이 속력부터 TurnRate로 돈다
		JumpSpeed = 6,			-- 높이 ≈ JumpSpeed² / 19.6
	},

	-- 균형형
	coupe = {},
	police = { MaxForwardSpeed = 22, BoostMaxSpeed = 32, Acceleration = 16, Grip = 11, TurnRate = 105 },

	-- 속도형: 빠르지만 덜 붙고 점프가 낮다
	italia = { MaxForwardSpeed = 23, BoostMaxSpeed = 33, Acceleration = 17, Grip = 8.5, TurnRate = 105 },
	lamb = { MaxForwardSpeed = 25, BoostMaxSpeed = 35, Acceleration = 18, Grip = 8, JumpSpeed = 5.2 },
	ghini = { MaxForwardSpeed = 23, BoostMaxSpeed = 33, Acceleration = 19, Grip = 9, TurnRate = 110, JumpSpeed = 5.5 },
	fenyr = { MaxForwardSpeed = 24, BoostMaxSpeed = 36, Acceleration = 16, Grip = 8.5, TurnRate = 90 },

	-- 가속형: 출발이 빠르고 잘 미끄러진다
	kamaro = { MaxForwardSpeed = 22, BoostMaxSpeed = 32, Acceleration = 20, Grip = 7, TurnRate = 95 },

	-- 기동형: 잘 돌고 잘 붙는다
	rally = { MaxForwardSpeed = 21, Acceleration = 17, Grip = 13, TurnRate = 120, FullTurnSpeed = 4, JumpSpeed = 6.5 },

	-- 험로형: 느리지만 높이 뛴다
	jeep = { MaxForwardSpeed = 18, BoostMaxSpeed = 27, Acceleration = 13, Grip = 12, TurnRate = 95, JumpSpeed = 7.5 },
	mobil = { MaxForwardSpeed = 21, BoostMaxSpeed = 34, Acceleration = 14, TurnRate = 90, JumpSpeed = 6.5 },

	-- 중량형: 느리고 둔하지만 단단히 붙는다
	armor = { MaxForwardSpeed = 17, BoostMaxSpeed = 26, Acceleration = 11, Deceleration = 24, Grip = 13, TurnRate = 85, JumpSpeed = 5.5 },
	van = { MaxForwardSpeed = 16, BoostMaxSpeed = 25, Acceleration = 11, Grip = 11, TurnRate = 85, JumpSpeed = 6.5 },
}

local Car = obj:AsVehicle()
local AppliedBody = nil

local function Axis(PositiveKey, NegativeKey)
	local Value = 0
	if IsKeyDown(PositiveKey) then Value = Value + 1 end
	if IsKeyDown(NegativeKey) then Value = Value - 1 end
	return Value
end

-- Default를 복사한 뒤 차체 표의 값으로 덮어쓴다. 차체 표에 없는 키는 Default 값이 된다.
local function BuildPreset(BodyName)
	local Result = {}
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

-- 처음 로드할 때와 핫 리로드할 때마다 다시 적용한다. (BeginPlay는 핫 리로드 때 다시 불리지 않는다)
if Car then ApplyPreset() end

function BeginPlay()
	if not Car then
		LogError("VehicleController.lua: not attached to a vehicle - " .. obj.Name)
		return
	end
	print(string.format("[Vehicle] %s PlayerIndex = %d", obj.Name, Car.PlayerIndex))
end

function EndPlay()
print("[EndPlay] " .. obj.UUID)
end

function Tick(dt)
	if not Car then return end

	-- 누가 SetBody로 차체를 바꿨든(차량 선택 등) 그 차체의 프리셋을 맞춘다.
	if Car.BodyName ~= AppliedBody then ApplyPreset() end

	local Keys = KeyBindings[Car.PlayerIndex]
	if not Keys then return end

	Car:SetInput(Axis(Keys.Forward, Keys.Backward), Axis(Keys.Right, Keys.Left), IsKeyDown(Keys.Boost))

	if IsKeyPressed(Keys.Jump) then
		Car:Jump()
	end
end
