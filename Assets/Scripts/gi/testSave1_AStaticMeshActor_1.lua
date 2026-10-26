function BeginPlay()
print("[BeginPlay] " .. obj.UUID)
obj:PrintLocation()
end

function EndPlay()
print("[EndPlay] " .. obj.UUID)
obj:PrintLocation()
end

function OnOverlap(OtherActor)
OtherActor:PrintLocation();
end

function OnHit(OtherActor)
OtherActor:PrintLocation();
end

function Tick(dt)
obj.Location = obj.Location + Vector(-1,0,0) * dt
obj:PrintLocation()
end