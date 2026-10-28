SetSoundVolume("wall", 0.4)
SetSoundPitch("wall", 1.0)

local ContactGap = 0.15
local MaxWallNormalZ = 0.5
local MaxTrackedSpeed = 200

local ElapsedTime = 0
local LastWallContactTime = -ContactGap
local PrevLocation = nil

local function SetSharedVelocity(Velocity)
	if Game then
		Game.BallVelocity = Velocity
	end
end

function BeginPlay()
	SetSharedVelocity(Vector(0, 0, 0))
end

function Tick(dt)
	ElapsedTime = ElapsedTime + dt
	local Location = obj.Location
	if PrevLocation and dt > 0 then
		local Estimated = (Location - PrevLocation) / dt
		if Estimated:Length() <= MaxTrackedSpeed then
			SetSharedVelocity(Estimated)
		else
			SetSharedVelocity(Vector(0, 0, 0))
		end
	end
	PrevLocation = Location
end

function OnHit(OtherActor, Normal)
	if OtherActor:AsVehicle() or math.abs(Normal.z) > MaxWallNormalZ then return end

	local bNewContact = ElapsedTime - LastWallContactTime >= ContactGap
	LastWallContactTime = ElapsedTime
	if bNewContact then
		PlaySound("wall")
	end
end
