function OnOverlap(OtherActor)
    if not OtherActor:IsBall() then return end
    if Game.OnGoal then
        Game.OnGoal()
    end
end
