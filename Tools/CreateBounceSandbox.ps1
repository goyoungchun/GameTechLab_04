param()
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$actors = [System.Collections.Generic.List[object]]::new()
$blue = @(0.15,0.45,0.80,1)
$orange = @(1,0.43,0.12,1)
$teal = @(0.12,0.80,0.62,1)
$yellow = @(0.95,0.73,0.20,1)
$slate = @(0.18,0.23,0.31,1)

function Transform($Position = @(0,0,0), $Rotation = @(0,0,0), $Scale = @(1,1,1)) {
    return [ordered]@{ Location=$Position; Rotation=$Rotation; Scale=$Scale }
}
function Component($Name, $Class, $Properties) {
    return [ordered]@{Class=$Class; Name=$Name; Properties=$Properties}
}
function Primitive($Position, $Rotation = @(0,0,0), $Scale = @(1,1,1), [int]$Motion = 0, [bool]$Gravity = $false, [int]$Response = 2) {
    return [ordered]@{
        Transform=(Transform $Position $Rotation $Scale)
        MotionType=$Motion; ObjectChannel=$(if ($Motion -eq 0) {0} else {1})
        WorldStatic=$Response; WorldDynamic=$Response; bEnableGravity=$Gravity
        bAutoActivate=$true; bVisible=$true; bHiddenInGame=$false
        bCastShadow=$true; bReceiveShadow=$true; bBackfaceDepth=$false
    }
}
function Visual($Name, $Mesh, $Position, $Scale, $Color, $Rotation = @(0,0,0)) {
    $p = Primitive $Position $Rotation $Scale 0 $false 0
    $p.StaticMesh=$Mesh
    $p.OverrideMaterials=@([ordered]@{
        Base='DefaultMaterial'; BaseColor=$Color; SpecularColor=@(0.12,0.12,0.12); Shininess=24
        Textures=@('WhiteTexture'); PSOType=0; SamplerState='LinearClamp'; UVScrollSpeed=@(0,0)
    })
    return (Component $Name 'UStaticMeshComponent' $p)
}
function Add-Actor($Parts, [bool]$Collision = $true, [string]$Class = 'AActor') {
    $actors.Add([ordered]@{Class=$Class; Properties=@{bActorEnableCollision=$Collision}; Components=@($Parts)})
}
function Add-Label($Name, $Position, $Text, [double]$Size = 0.36) {
    $p = Primitive $Position @(0,0,0) @(1,1,1) 0 $false 0
    $p.Text=$Text; $p.TextSize=$Size; $p.Font='Fonts/CookieRun.json'
    $p.bCastShadow=$false; $p.bReceiveShadow=$false
    Add-Actor @((Component $Name 'UTextRenderComponent' $p)) $false
}
function Add-Mark($Name, $Position, $Scale, $Color) {
    Add-Actor @((Visual $Name 'Cube' $Position $Scale $Color)) $false
}
function Add-Box($Name, $Position, $Extent, $Color = $blue, $Rotation = @(0,0,0), [int]$Motion = 0, [bool]$Gravity = $false) {
    $p = Primitive $Position $Rotation @(1,1,1) $Motion $Gravity
    $p.BoxExtent=$Extent; $p.ShapeColor=$Color; $p.bDrawShape=$false
    Add-Actor @(
        (Component $Name 'UBoxComponent' $p),
        (Visual ($Name+'_Visual') 'Cube' @(0,0,0) @(($Extent[0]*2),($Extent[1]*2),($Extent[2]*2)) $Color)
    )
}
function Add-Ball($Name, $Position, [double]$Radius = 0.6) {
    $p = Primitive $Position @(0,0,0) @(1,1,1) 2 $true
    $p.SphereRadius=$Radius; $p.ShapeColor=$orange; $p.bDrawShape=$false
    Add-Actor @(
        (Component $Name 'USphereComponent' $p),
        (Visual ($Name+'_Visual') 'Sphere' @(0,0,0) @($Radius,$Radius,$Radius) $orange)
    )
}

# Every orange ball is a Dynamic Sphere handled by the Solver. No ProjectileMovement or scripts.
Add-Box 'Arena_Floor' @(14,0,-0.4) @(20,15,0.4) @(0.075,0.105,0.15,1)
Add-Box 'Arena_FarWall' @(33.5,0,2) @(0.35,14.7,2) $slate
Add-Box 'Arena_BackWall' @(14,14.5,2) @(19.5,0.35,2) $slate
Add-Box 'Arena_FrontRail' @(14,-14.5,0.55) @(19.5,0.2,0.55) $slate
Add-Box 'Arena_LeftRail' @(-5.5,0,0.55) @(0.2,14.7,0.55) $slate
Add-Mark 'Arena_Divider' @(18,-0.3,0.015) @(0.08,27,0.03) $teal
foreach ($y in @(-4,4)) { Add-Mark ('Bay_Line_'+$y) @(8,$y,0.015) @(19,0.06,0.03) $slate }
Add-Label 'Arena_Title' @(11,-13,12.4) 'SPHERE x BOX | BOUNCE SANDBOX' 0.85
Add-Label 'Arena_Controls' @(11,-13,11.2) 'Play | F8 inspect + edit | Pause / Step | Stop + Play reset' 0.40
Add-Label 'Arena_Legend' @(12,13,5.2) 'ORANGE: dynamic sphere | BLUE: fixed box | GREEN: movable box' 0.42

# A: identical radius, three drop clearances above the box top. All pad tops are Z=0.6.
for ($i=0; $i -lt 3; $i++) {
    $x=2+6*$i; $height=3+3*$i; $centerZ=$height+1.2
    Add-Box ('Drop_'+$height+'m_Pad') @($x,-8,0.3) @(2,2,0.3)
    Add-Ball ('Drop_'+$height+'m_Sphere') @($x,-8,$centerZ)
    Add-Label ('Drop_'+$height+'m_Label') @($x,-10.2,0.65) ($height.ToString()+' m DROP | r = 0.6') 0.34
    Add-Mark ('Drop_'+$height+'m_ReleaseLine') @($x,-10.4,$centerZ) @(3.8,0.045,0.045) $yellow
    Add-Mark ('Drop_'+$height+'m_Gauge') @(($x+2.2),-10.4,($centerZ/2)) @(0.04,0.04,$centerZ) $yellow
}
Add-Label 'Drop_Title' @(8,-8,11.8) '01 | DROP HEIGHT / REBOUND' 0.48

# B: sloped faces turn downward speed into different reflected directions.
for ($i=0; $i -lt 3; $i++) {
    $x=2+6*$i; $angle=15+15*$i
    Add-Box ('Angle_'+$angle+'_Ramp') @($x,0,1.4) @(1.8,1.7,0.25) $blue @($angle,0,0)
    Add-Ball ('Angle_'+$angle+'_Sphere') @($x,0,6.5)
    Add-Label ('Angle_'+$angle+'_Label') @($x,-2.4,0.3) ($angle.ToString()+' deg BOX') 0.34
}
Add-Label 'Angle_Title' @(8,0,9) '02 | CHANGE THE REFLECTION ANGLE' 0.48

# C: a floating dynamic box accepts momentum; a grounded dynamic box also contacts the floor.
Add-Box 'Momentum_FloatingBox' @(3,8,2.2) @(0.85,0.85,0.85) $teal @(0,0,0) 2 $false
Add-Ball 'Momentum_Floating_Sphere' @(3,8,7)
Add-Label 'Momentum_Floating_Label' @(3,5.8,0.3) 'FREE BOX | gravity OFF' 0.34
Add-Box 'Momentum_GroundedBox' @(11,8,0.85) @(0.85,0.85,0.85) $teal @(0,0,0) 2 $true
Add-Ball 'Momentum_Grounded_Sphere' @(11,8,7)
Add-Label 'Momentum_Grounded_Label' @(11,5.8,0.3) 'BOX + FLOOR CONTACT' 0.34
Add-Label 'Momentum_Title' @(8,8,9) '03 | MOVABLE BOX / MOMENTUM' 0.48

# D: open space and loose props for editing positions, uniform ball scale and box rotation.
Add-Box 'Free_Ramp' @(22,7,1.4) @(2.4,2,0.3) $blue @(25,25,0)
Add-Box 'Free_Wall' @(29,5,1.5) @(0.6,2.5,1.5) $blue @(0,25,0)
Add-Box 'Free_DynamicBox_A' @(25,-6,0.85) @(0.85,0.85,0.85) $teal @(0,20,0) 2 $true
Add-Box 'Free_DynamicBox_B' @(29,-1,0.6) @(1.1,0.65,0.6) $teal @(0,-20,0) 2 $true
Add-Ball 'Free_Sphere_A' @(25,-6,5) 0.8
Add-Ball 'Free_Sphere_B' @(22,7,6.5) 0.6
Add-Ball 'Free_Sphere_C' @(24,1,4.5) 0.45
Add-Label 'Free_Title' @(26,-3,10) '04 | FREE PLAY AREA' 0.60
Add-Label 'Free_Hint' @(25,-10,1.6) 'Reposition balls / rotate boxes / try different sizes' 0.36

foreach ($light in @(
    @{Name='Sandbox_Ambient'; Class='UAmbientLightComponent'; Intensity=0.32; Rotation=@(0,0,0); Color=@(0.88,0.93,1)},
    @{Name='Sandbox_Sun'; Class='UDirectionalLightComponent'; Intensity=1.6; Rotation=@(55,-35,0); Color=@(1,0.96,0.9)}
)) {
    $p=[ordered]@{Transform=(Transform @(14,0,18) $light.Rotation); Intensity=$light.Intensity
        LightColor=$light.Color; bEnabled=$true; bAutoActivate=$true; bVisible=$true; bHiddenInGame=$false
        CascadeCount=3; CascadeExponential=3; CascadeBlendRange=3; bCascadeDebug=$false}
    Add-Actor @((Component $light.Name $light.Class $p)) $false
}
# APawn provides a fixed overview. F8 in Selected Viewport enables the existing editor inspection camera.
Add-Actor @(
    (Component 'RootComponent_0' 'USceneComponent' @{Transform=(Transform @(-26,-29,27) @(26,36,0)); bAutoActivate=$true; bVisible=$true; bHiddenInGame=$false}),
    (Component 'BounceSandbox_OverviewCamera' 'UCameraComponent' @{Transform=(Transform); bAutoActivate=$true; bVisible=$true; bHiddenInGame=$false})
) $false 'APawn'

$path = Join-Path $repoRoot 'Scenes/05_SphereBoxBounceSandbox.scene'
[ordered]@{Version=2; Actors=$actors.ToArray()} | ConvertTo-Json -Depth 50 | Set-Content -LiteralPath $path -Encoding utf8NoBOM
Write-Output ('Created {0} ({1} actors)' -f $path,$actors.Count)
