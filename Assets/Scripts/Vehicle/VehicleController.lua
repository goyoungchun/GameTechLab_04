
_G.KeyBindings = _G.KeyBindings or {
	[0] = { Forward = Key.W, Backward = Key.S, Left = Key.A, Right = Key.D, Boost = Key.LShift, Jump = Key.LControl },
	[1] = { Forward = Key.Up, Backward = Key.Down, Left = Key.Left, Right = Key.Right, Boost = Key.RShift, Jump = Key.RControl },
}

local Car = obj:AsVehicle()

local function Axis(PositiveKey, NegativeKey)
	local Value = 0
	if IsKeyDown(PositiveKey) then Value = Value + 1 end
	if IsKeyDown(NegativeKey) then Value = Value - 1 end
	return Value
end

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
	local Keys = KeyBindings[Car.PlayerIndex]
	if not Keys then return end

	Car:SetInput(Axis(Keys.Forward, Keys.Backward), Axis(Keys.Right, Keys.Left), IsKeyDown(Keys.Boost))

	if IsKeyPressed(Keys.Jump) then
		Car:Jump()
	end

end