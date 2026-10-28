
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

local ElapsedTime = 0
local bBoostSoundPlaying = false
local bDriveSoundPlaying = false
local DriveReferenceSpeed = 30
local ContactGap = 0.15
local MaxWallNormalZ = 0.5
local LastWallContactTime = -ContactGap
_G.PreviousCarVelocities = _G.PreviousCarVelocities or {}

local ImpactSounds = {
	Car  = { Sound = "crash", MinSpeed = 4, MaxSpeed = 40, MinVolume = 0.15,  MaxVolume = 1, LastContactTime = -ContactGap },
	Ball = { Sound = "kick",  MinSpeed = 2, MaxSpeed = 40, MinVolume = 1 / 3, MaxVolume = 1, LastContactTime = -ContactGap },
}

local function UpdateDriveSound(bEnabled)
	local SoundName = "drive_" .. (Car.PlayerIndex + 1)
	if not bEnabled then
		if bDriveSoundPlaying then
			StopSound(SoundName)
			bDriveSoundPlaying = false
		end
		return
	end

	local SpeedRatio = math.min(Car.Speed / DriveReferenceSpeed, 1.5)
	SetSoundVolume(SoundName, 0.2 + 0.35 * math.min(SpeedRatio, 1))
	SetSoundPitch(SoundName, 0.7 + 0.8 * SpeedRatio)
	if not bDriveSoundPlaying then
		PlayBGM(SoundName)
		bDriveSoundPlaying = true
	end
end

local function PlayImpactSound(Impact, ImpactSpeed)
	local bNewContact = ElapsedTime - Impact.LastContactTime >= ContactGap
	Impact.LastContactTime = ElapsedTime
	if not bNewContact or ImpactSpeed < Impact.MinSpeed then return end

	local Ratio = math.min((ImpactSpeed - Impact.MinSpeed) / (Impact.MaxSpeed - Impact.MinSpeed), 1)
	SetSoundVolume(Impact.Sound, Impact.MinVolume + (Impact.MaxVolume - Impact.MinVolume) * Ratio)
	PlaySound(Impact.Sound)
end

local function SetBoostSoundPlaying(bPlaying)
	if bPlaying == bBoostSoundPlaying then return end
	bBoostSoundPlaying = bPlaying
	local SoundName = "boost_" .. (Car.PlayerIndex + 1)
	if bPlaying then
		PlaySound(SoundName)
	else
		StopSound(SoundName)
	end
end

function OnHit(OtherActor, Normal)
	if not Car then return end
	local MyVelocity = PreviousCarVelocities[Car.PlayerIndex] or Car.Velocity
	local OtherCar = OtherActor:AsVehicle()
	if OtherCar then
		if Car.PlayerIndex < OtherCar.PlayerIndex then
			local OtherVelocity = PreviousCarVelocities[OtherCar.PlayerIndex] or OtherCar.Velocity
			PlayImpactSound(ImpactSounds.Car, (MyVelocity - OtherVelocity):Length())
		end
	elseif OtherActor:IsBall() then
		local BallVelocity = Game and Game.BallVelocity or Vector(0, 0, 0)
		PlayImpactSound(ImpactSounds.Ball, (MyVelocity - BallVelocity):Length())
	elseif math.abs(Normal.z) <= MaxWallNormalZ then
		local bNewContact = ElapsedTime - LastWallContactTime >= ContactGap
		LastWallContactTime = ElapsedTime
		if bNewContact then
			PlaySound("wall")
		end
	end
end

function EndPlay()
print("[EndPlay] " .. obj.UUID)
end

function Tick(dt)
	ElapsedTime = ElapsedTime + dt

	if not Car then return end

	PreviousCarVelocities[Car.PlayerIndex] = Car.Velocity

	BindCarModel()

	-- 누가 SetBody로 차체를 바꿨든(차량 선택 등) 그 차체의 프리셋을 맞춘다.
	if Car.BodyName ~= AppliedBody then ApplyPreset() end

	local Keys = KeyBindings[Car.PlayerIndex]
	if not Keys then return end

	local InputEnabled = Game.CarInputEnabled and Game.CarInputEnabled[Car.PlayerIndex + 1]
	if InputEnabled == false then
		Car:SetInput(0, 0, false)
		SetBoostSoundPlaying(false)
		UpdateDriveSound(false)
		return
	end

	local bBoostPressed = IsKeyDown(Keys.Boost)
	Car:SetInput(Axis(Keys.Forward, Keys.Backward), Axis(Keys.Right, Keys.Left), bBoostPressed)
	SetBoostSoundPlaying(bBoostPressed and Car.BoostRatio > 0)
	UpdateDriveSound(true)

	if IsKeyPressed(Keys.Jump) then
		if Car.IsGrounded then
			PlaySound("jump")
		end
		Car:Jump()
	end
end
