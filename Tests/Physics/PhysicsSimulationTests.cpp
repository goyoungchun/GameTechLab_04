#include "EnginePCH.h"
#include "Collision/FPBDRigidsEvolutionBase.h"
#include "Component/BoxComponent.h"
#include "Component/CapsuleComponent.h"
#include "Engine/World.h"
#include "ObjectSystem/ObjectFactory.h"
#include "Asset/AssetManager.h"
#include "Asset/ObjImporter/ObjImporter.h"
#include "Camera/CameraComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/ScriptComponent.h"
#include "Engine/Level.h"
#include "GameFramework/Vehicle/VehiclePawn.h"
#include "GameFramework/Vehicle/VehicleMovementComponent.h"
#include "Collision/FPhysSceneChaos.h"
#include "Collision/FPBDRigidsSolver.h"
#include "Render/GeometryGenerator.h"
#include "Serialization/JsonArchive.h"
#include <chrono>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <optional>
#include <random>
#include <stdexcept>

namespace
{
    void Require(bool Value, const char* Message)
    {
        if (!Value) throw std::runtime_error(Message);
    }
    void Near(double Actual, double Expected, const char* Message, double Tolerance = 2e-4)
    {
        if (!std::isfinite(Actual) || std::abs(Actual - Expected) > Tolerance)
            throw std::runtime_error(std::string(Message) + ": actual=" + std::to_string(Actual)
                + ", expected=" + std::to_string(Expected));
    }
    FTransform Pose(FVector Position = {}, FRotator Rotation = {}, FVector Scale = FVector::OneVector)
    {
        return FTransform(Rotation, Position, Scale);
    }

    struct FFixture
    {
        UWorld World;
        UBoxComponent* Box = FObjectFactory::ConstructObject<UBoxComponent>();
        UCapsuleComponent* A = FObjectFactory::ConstructObject<UCapsuleComponent>();
        UCapsuleComponent* B = FObjectFactory::ConstructObject<UCapsuleComponent>();
        FFixture()
        {
            Box->SetBoxExtent(FVector(1.0f));
            A->SetCapsuleSize(0.5f, 1.5f);
            B->SetCapsuleSize(0.5f, 1.5f);
            for (UPrimitiveComponent* Component : std::array<UPrimitiveComponent*, 3>{ Box, A, B })
                Component->RegisterComponentWithWorld(&World);
        }
        ~FFixture() { delete B; delete A; delete Box; }

        std::optional<FPhysicsCollisionResult> Detect(UPrimitiveComponent* First, const FTransform& FirstPose,
            UPrimitiveComponent* Second, const FTransform& SecondPose, bool bBlock = false,
            FTransform* OutResolved = nullptr)
        {
            FPBDRigidsEvolutionGBF Evolution;
            Evolution.RegisterBody(1, First);
            Evolution.RegisterBody(2, Second);
            TArray<FPhysicsBodyState> States;
            FPhysicsBodyState State;
            State.BodyId = 1;
            State.WorldTransform = FirstPose;
            State.MotionType = EPhysicsMotionType::Static;
            State.bEnableGravity = false;
            if (!bBlock) State.CollisionResponses.WorldStatic = ECollisionResponse::Overlap;
            States.Add(State);
            State.BodyId = 2;
            State.WorldTransform = SecondPose;
            State.MotionType = bBlock ? EPhysicsMotionType::Dynamic : EPhysicsMotionType::Kinematic;
            States.Add(State);
            Evolution.AdvanceOneTimeStep(States, 1.0f / 60.0f);
            if (OutResolved) *OutResolved = States[1].WorldTransform;
            if (Evolution.GetCollisionResults().IsEmpty()) return std::nullopt;
            Require(Evolution.GetCollisionResults().Num() == 1, "One unique collision pair");
            const auto Result = Evolution.GetCollisionResults()[0];
            Near(Result.Normal.Length(), 1.0, "Unit normal");
            Require(std::isfinite(Result.ContactPoint.X) && std::isfinite(Result.ContactPoint.Y)
                && std::isfinite(Result.ContactPoint.Z) && std::isfinite(Result.PenetrationDepth)
                && Result.PenetrationDepth >= 0.0f, "Finite contact");
            return Result;
        }
    };

    // Independent distance oracle: ternary minimization of the convex distance function,
    // rather than the derivative / stationary-point algorithms used by the Solver.
    double SquaredDistance(FVector A, FVector B)
    {
        const double X = double(A.X) - B.X, Y = double(A.Y) - B.Y, Z = double(A.Z) - B.Z;
        return X * X + Y * Y + Z * Z;
    }
    template<typename F> double Minimize(F&& Function)
    {
        double Lower = 0.0, Upper = 1.0;
        for (int Iteration = 0; Iteration < 90; ++Iteration)
        {
            const double Left = (2 * Lower + Upper) / 3, Right = (Lower + 2 * Upper) / 3;
            if (Function(Left) < Function(Right)) Upper = Right;
            else Lower = Left;
        }
        return std::min({ Function(0.0), Function(1.0), Function((Lower + Upper) / 2) });
    }
    FVector Lerp(FVector A, FVector B, double T)
    {
        return FVector(float(A.X * (1 - T) + B.X * T), float(A.Y * (1 - T) + B.Y * T), float(A.Z * (1 - T) + B.Z * T));
    }
    struct FSegment { FVector Start, End; double Radius; };
    FSegment Capsule(const UCapsuleComponent* Shape, const FTransform& Transform)
    {
        const FVector Scale = Transform.Scale.GetAbs();
        const float Radius = Shape->GetCapsuleRadius() * std::min({ Scale.X, Scale.Y, Scale.Z });
        const FVector Offset = Transform.GetOrientation().Normalized().RotateVector(FVector(0, 0, 1))
            * std::max(0.0f, Shape->GetCapsuleHalfHeight() * Scale.Z - Radius);
        return { Transform.Location - Offset, Transform.Location + Offset, Radius };
    }
    double PointSegmentDistance(FVector P, const FSegment& Segment)
    {
        const FVector D = Segment.End - Segment.Start;
        const double DD = double(D.X) * D.X + double(D.Y) * D.Y + double(D.Z) * D.Z;
        const double Projection = (double(P.X) - Segment.Start.X) * D.X
            + (double(P.Y) - Segment.Start.Y) * D.Y + (double(P.Z) - Segment.Start.Z) * D.Z;
        return SquaredDistance(P, Lerp(Segment.Start, Segment.End, DD > 0 ? std::clamp(Projection / DD, 0.0, 1.0) : 0));
    }

    void CheckSeparation(FFixture& F, UPrimitiveComponent* A, FTransform TA,
        UPrimitiveComponent* B, FTransform TB, const FPhysicsCollisionResult& Contact)
    {
        // Applying the reported normal and depth must actually separate the original shapes.
        TB.Location += Contact.Normal * (Contact.PenetrationDepth + 0.002f);
        Require(!F.Detect(A, TA, B, TB), "Reported correction separates shapes");
    }

    void FixedCases(FFixture& F)
    {
        const FTransform Origin = Pose();
        auto Contact = F.Detect(F.Box, Origin, F.A, Pose(FVector(1.3f, 0, 0)));
        Require(Contact.has_value(), "BC face contact");
        Near(Contact->PenetrationDepth, 0.2, "BC face depth");
        Near(Contact->Normal.X, 1, "BC normal A to B");
        Contact = F.Detect(F.Box, Origin, F.A, Pose(FVector(1.5f, 0, 0)));
        Require(Contact.has_value(), "BC exact tangency");
        Near(Contact->PenetrationDepth, 0, "BC tangent depth");
        Contact = F.Detect(F.Box, Origin, F.A, Pose(FVector(1.3f, 1.3f, 0)));
        Require(Contact.has_value(), "BC edge contact");
        Near(Contact->PenetrationDepth, 0.5 - std::sqrt(0.18), "BC edge depth");
        Require(!F.Detect(F.Box, Origin, F.A, Pose(FVector(1.4f, 1.4f, 0))), "BC corner miss despite overlapping AABBs");
        Contact = F.Detect(F.Box, Origin, F.A, Pose(FVector(0, 0, 2.4f)));
        Require(Contact.has_value(), "BC hemisphere endpoint");
        Near(Contact->PenetrationDepth, 0.1, "BC endpoint depth");
        Contact = F.Detect(F.Box, Origin, F.A, Origin);
        Require(Contact.has_value(), "BC containment");
        Near(Contact->PenetrationDepth, 1.5, "BC containment depth includes box interior");
        CheckSeparation(F, F.Box, Origin, F.A, Origin, *Contact);

        F.Box->SetBoxExtent(FVector(1, 1, 10));
        F.A->SetCapsuleSize(0.25f, 4);
        const FTransform Diagonal = Pose({}, FRotator(90, 45, 0));
        Contact = F.Detect(F.Box, Origin, F.A, Diagonal);
        Require(Contact.has_value(), "BC through-box diagonal");
        Near(Contact->PenetrationDepth, std::sqrt(2.0) + 0.25, "BC edge-cross-axis minimum translation");
        CheckSeparation(F, F.Box, Origin, F.A, Diagonal, *Contact);
        F.Box->SetBoxExtent(FVector(1));
        F.A->SetCapsuleSize(0.5f, 1.5f);

        Contact = F.Detect(F.A, Origin, F.B, Pose(FVector(0.8f, 0, 0)));
        Require(Contact.has_value(), "CC parallel sides");
        Near(Contact->PenetrationDepth, 0.2, "CC parallel depth");
        Contact = F.Detect(F.A, Origin, F.B, Pose(FVector(1, 0, 0)));
        Require(Contact.has_value(), "CC exact tangency");
        Near(Contact->PenetrationDepth, 0, "CC tangent depth");
        Contact = F.Detect(F.A, Origin, F.B, Pose(FVector(0, 0, 2.8f)));
        Require(Contact.has_value(), "CC endpoint pair");
        Near(Contact->PenetrationDepth, 0.2, "CC endpoint depth");
        const FTransform Crossing = Pose({}, FRotator(90, 0, 0));
        Contact = F.Detect(F.A, Origin, F.B, Crossing);
        Require(Contact.has_value(), "CC crossing axes");
        Near(Contact->PenetrationDepth, 1.0, "CC crossing depth");
        Near(std::abs(Contact->Normal.Y), 1.0, "CC crossing normal perpendicular to both segments");
        CheckSeparation(F, F.A, Origin, F.B, Crossing, *Contact);
        Contact = F.Detect(F.A, Origin, F.B, Origin);
        Require(Contact.has_value(), "CC coincident axes");
        CheckSeparation(F, F.A, Origin, F.B, Origin, *Contact);

        F.A->SetCapsuleSize(0.1f, 100000.1f);
        F.B->SetCapsuleSize(0.1f, 100000.1f);
        const FTransform NearParallel = Pose(FVector(0.01f, 0, 10000), FRotator(0.00001f, 0, 0));
        Contact = F.Detect(F.A, Origin, F.B, NearParallel);
        Require(Contact.has_value(), "CC long nearly parallel segments intersect internally");
        Near(Contact->PenetrationDepth, 0.2, "CC near-parallel depth");
        Near(std::abs(Contact->Normal.Y), 1, "CC near-parallel crossing normal");
        CheckSeparation(F, F.A, Origin, F.B, NearParallel, *Contact);

        F.A->SetCapsuleSize(0.5f, 0.5f);
        F.B->SetCapsuleSize(0.5f, 0.5f);
        Contact = F.Detect(F.A, Origin, F.B, Origin);
        Require(Contact.has_value(), "CC two degenerate segments");
        Near(Contact->PenetrationDepth, 1, "CC sphere degeneration");
        CheckSeparation(F, F.A, Origin, F.B, Origin, *Contact);
        F.B->SetCapsuleSize(0.5f, 1.5f);
        Contact = F.Detect(F.A, Origin, F.B, Origin);
        Require(Contact.has_value(), "CC one degenerate segment");
        CheckSeparation(F, F.A, Origin, F.B, Origin, *Contact);
        Contact = F.Detect(F.Box, Origin, F.A, Origin);
        Require(Contact.has_value(), "BC degenerate segment");
        CheckSeparation(F, F.Box, Origin, F.A, Origin, *Contact);
        F.A->SetCapsuleSize(0.5f, 1.5f);

        FTransform Resolved;
        F.Detect(F.Box, Origin, F.A, Pose(FVector(0, 0, 2)), true, &Resolved);
        Near(Resolved.Location.Z, 2.5, "BC Solver moves dynamic capsule out of static box");
        F.Detect(F.A, Origin, F.B, Pose(FVector(0.8f, 0, 0)), true, &Resolved);
        Near(Resolved.Location.X, 1, "CC Solver moves dynamic capsule out of static capsule");
        F.Detect(F.Box, Origin, F.A, Origin, true, &Resolved);
        Near(Resolved.Location.Length(), 1.5, "BC Solver resolves containment");
        Require(!F.Detect(F.Box, Origin, F.A, Pose({}, {}, FVector(0, 1, 1))), "Zero capsule radius rejected");
        Require(!F.Detect(F.A, Origin, F.B, Pose(FVector(std::numeric_limits<float>::quiet_NaN(), 0, 0))), "Nonfinite pose rejected");
    }

    void RandomCases(FFixture& F)
    {
        std::mt19937 Random(20261025);
        auto Value = [&](float Low, float High) { return std::uniform_real_distribution<float>(Low, High)(Random); };
        auto RandomPose = [&]
        {
            return Pose(FVector(Value(-3, 3), Value(-3, 3), Value(-3, 3)),
                FRotator(Value(-180, 180), Value(-180, 180), Value(-180, 180)),
                FVector(-Value(0.4f, 1.8f), Value(0.4f, 1.8f), Value(0.4f, 1.8f)));
        };
        int BCHits = 0, CCHits = 0;
        for (int Iteration = 0; Iteration < 1000; ++Iteration)
        {
            const FTransform TA = RandomPose(), TB = RandomPose();
            F.A->SetCapsuleSize(Value(0.2f, 1), Value(1, 3));
            F.B->SetCapsuleSize(Value(0.2f, 1), Value(1, 3));
            const FSegment A = Capsule(F.A, TA), B = Capsule(F.B, TB);
            const double CCDistance = std::sqrt(Minimize([&](double T) { return PointSegmentDistance(Lerp(A.Start, A.End, T), B); }));
            auto CC = F.Detect(F.A, TA, F.B, TB);
            Require(CC.has_value() == (CCDistance <= A.Radius + B.Radius), "CC agrees with independent distance oracle");
            if (CC)
            {
                ++CCHits;
                Near(CC->PenetrationDepth, A.Radius + B.Radius - CCDistance, "CC random depth");
                CheckSeparation(F, F.A, TA, F.B, TB, *CC);
                const auto Reverse = F.Detect(F.B, TB, F.A, TA);
                Require(Reverse.has_value(), "CC reverse ordering");
                Near(Reverse->Normal.Dot(CC->Normal), -1, "CC reverse normal");
            }

            const FQuat Inverse = TA.GetOrientation().Normalized().Inverse();
            const FVector Start = Inverse.RotateVector(B.Start - TA.Location), End = Inverse.RotateVector(B.End - TA.Location);
            const FVector Extent = F.Box->GetBoxExtent() * TA.Scale.GetAbs();
            const double BCDistance = std::sqrt(Minimize([&](double T)
            {
                const FVector P = Lerp(Start, End, T);
                const FVector Q(std::clamp(P.X, -Extent.X, Extent.X), std::clamp(P.Y, -Extent.Y, Extent.Y), std::clamp(P.Z, -Extent.Z, Extent.Z));
                return SquaredDistance(P, Q);
            }));
            auto BC = F.Detect(F.Box, TA, F.B, TB);
            Require(BC.has_value() == (BCDistance <= B.Radius), "BC agrees with independent distance oracle");
            if (BC)
            {
                ++BCHits;
                if (BCDistance > 1e-5) Near(BC->PenetrationDepth, B.Radius - BCDistance, "BC random depth");
                CheckSeparation(F, F.Box, TA, F.B, TB, *BC);
                const auto Reverse = F.Detect(F.B, TB, F.Box, TA);
                Require(Reverse.has_value(), "BC reverse ordering");
                Near(Reverse->Normal.Dot(BC->Normal), -1, "BC reverse normal");
            }
        }
        Require(BCHits > 100 && CCHits > 50, "Random suite exercises enough contacts");
        std::cout << "Random contacts: BC=" << BCHits << ", CC=" << CCHits << '\n';
    }
}

void VehicleVisualCases()
{
    struct FTestLog : ILogSink
    {
        FTestLog() { FLog::AddSink(this); }
        ~FTestLog() { FLog::RemoveSink(this); }
        void OnLog(ELogVerbosity Verbosity,const FString& Message) override
        { if (Verbosity == ELogVerbosity::Warning || Verbosity == ELogVerbosity::Error) std::cerr << Message << '\n'; }
    } Log;
    const fs::path Temporary = "Build/VehicleVisualValidation";
    fs::create_directories(Temporary);
    const FString Obj = (Temporary / "cache.obj").generic_string();
    auto Write = [&](float X) { std::ofstream(Obj) << "o triangle\nv 0 0 0\nv " << X << " 0 0\nv 0 1 0\nf 1 2 3\n"; };
    Write(1);
    const auto Time = fs::file_time_type::clock::now();
    fs::last_write_time(Obj, Time);
    Require(FObjImporter::LoadStaticMeshData(Obj) != nullptr, "Initial cache writes");
    Write(3);
    fs::last_write_time(Obj, Time + std::chrono::seconds(2));
    auto Updated = FObjImporter::LoadStaticMeshData(Obj);
    Require(Updated != nullptr, "Updated source loads");
    Near(Updated->AABB.Max.Y, 3, "OBJ newer than its bake invalidates stale model orientation/geometry");

    FGeometryGenerator::CreateDefaultMeshDatas();
    auto* Cube = FObjectFactory::ConstructObject<UStaticMesh>();
    Cube->MeshData = *FGeometryGenerator::GetMeshData("Cube"); Cube->RebuildRenderData();
    UAssetManager::Get().RegisterAsset("Cube", Cube);
    const char* Cars[] = { "armor", "coupe", "fenyr", "ghini", "italia", "jeep", "kamaro", "lamb", "mobil", "police", "rally", "van" };
    for (const char* Name : Cars)
    {
        const FString Key = FString("Models/Cars/") + Name + ".obj";
        auto Data = FObjImporter::LoadStaticMeshData("Assets/" + Key);
        Require(Data != nullptr, "Current car OBJ loads");
        Near((Data->AABB.Min+Data->AABB.Max).Length(), 0, "Current converted car has centered origin", 0.00001);
        auto* Mesh = FObjectFactory::ConstructObject<UStaticMesh>();
        Mesh->MeshData = *Data; Mesh->RebuildRenderData();
        UAssetManager::Get().RegisterAsset(Key, Mesh);
        TArray<FStaticMeshPart> Parts, Cached;
        Require(UAssetManager::GetObjMeshParts(Mesh, Parts) && Parts.Num() == 5, "Each selectable car supplies a body and four wheels");
        Require(UAssetManager::GetObjMeshParts(Mesh, Cached) && Parts[0].Mesh == Cached[0].Mesh, "Car parts are shared between vehicles");
    }
    UWorld World;
    Require(World.Init(), "Vehicle visual world initializes");
    World.GetMainCamera()->GetCameraComponent()->SetExternalInputManaged(true);
    auto* Car = World.SpawnActor<AVehiclePawn>();
    for (auto* Component : Car->GetComponents())
        if (auto* Script = Cast<UScriptComponent>(Component)) Script->SetScriptPath("");
    Require(Car->SetBody("lamb"), "Editor model selection applies");
    const auto ScenePath = (Temporary / "vehicle.scene").generic_string();
    Require(FJsonArchive::SaveWorld(&World, ScenePath), "Selected car saves");
    json Saved; std::ifstream(ScenePath) >> Saved;
    for (const auto& Component : Saved["Actors"][0]["Components"])
        Require(!Component["Name"].get<FString>().starts_with("VehicleBodyVisual") && !Component["Name"].get<FString>().starts_with("Wheel"), "Generated visuals are rebuilt rather than saved as file assets");
    Require(FJsonArchive::LoadWorld(&World, ScenePath), "Selected car reloads");
    Car = Cast<AVehiclePawn>(World.GetPersistentLevel()->GetActors()[0]);
    Require(Car && Car->GetBodyName() == "lamb", "Reload preserves the selected original car name for Lua presets");
    {
        UWorld Copy;
        Require(Copy.DuplicateWorld(&World), "PIE world duplicates selected vehicle");
        auto* Duplicate = Cast<AVehiclePawn>(Copy.GetPersistentLevel()->GetActors()[0]);
        Require(Duplicate && Duplicate->GetBodyName() == "lamb", "PIE preserves the selected source mesh");
        Copy.BeginPlay();
        Require(!Duplicate->GetBodyMeshComponent()->IsVisible() && Car->GetBodyMeshComponent()->IsVisible(), "PIE visual changes do not hide the editor vehicle");
        Copy.EndPlay(); Copy.ClearWorld();
    }
    World.BeginPlay();
    auto* Body = Cast<UBoxComponent>(Car->GetRootComponent());
    Body->SetEnableGravity(false); Body->SetCOF(0);
    UCameraComponent* Camera = nullptr;
    std::array<UStaticMeshComponent*,4> Wheels{};
    UStaticMeshComponent* BodyVisual = nullptr;
    for (auto* C : Car->GetComponents())
    {
        if (auto* View = Cast<UCameraComponent>(C)) Camera = View;
        if (auto* Mesh = Cast<UStaticMeshComponent>(C))
        {
            if (C->GetName().starts_with("VehicleBodyVisual")) BodyVisual = Mesh;
            if (C->GetName().starts_with("WheelFrontLeft")) Wheels[0] = Mesh;
            if (C->GetName().starts_with("WheelFrontRight")) Wheels[1] = Mesh;
            if (C->GetName().starts_with("WheelRearLeft")) Wheels[2] = Mesh;
            if (C->GetName().starts_with("WheelRearRight")) Wheels[3] = Mesh;
        }
    }
    Require(Camera && BodyVisual && std::all_of(Wheels.begin(),Wheels.end(),[](auto* W){return W != nullptr;}), "Pawn owns camera and all visual components");
    int Index = 0;
    for (const char* Name : Cars)
    {
        Require(Car->SetBody(Name), "Runtime selection applies");
        Require(Car->GetBodyName() == Name, "Selected source name remains stable");
        Car->SetActorRotation(FRotator(0,Index++*30.0f,0));
        Car->SetActorScale3D(FVector(2)); Car->Tick(0);
        Require(!Car->GetBodyMeshComponent()->IsVisible() && BodyVisual->IsVisible(), "Original full mesh is replaced by the selected split body");
        FBox Bounds = BodyVisual->GetMeshData()->AABB.GetWorldAABB(BodyVisual->GetTransform().GetLocalMatrix());
        uint32 Indices = BodyVisual->GetMeshData()->Indices.Num();
        for (int I=0; I<4; ++I)
        {
            auto* Wheel = Wheels[I];
            Require(Wheel->IsVisible() && Wheel->GetStaticMesh(), "Selected wheel is visible");
            Require((Wheel->GetRelativeLocation().X>0) == (I<2), "Front/rear wheels follow the converted model direction");
            Require(Wheel->GetCollisionResponseToChannel(ECollisionChannel::WorldDynamic) == ECollisionResponse::Ignore, "Wheel visuals add no collision");
            Bounds.Expand(Wheel->GetMeshData()->AABB.GetWorldAABB(Wheel->GetTransform().GetLocalMatrix()));
            Indices += Wheel->GetMeshData()->Indices.Num();
        }
        const auto* Source = Car->GetBodyMeshComponent();
        const FBox Original = Source->GetMeshData()->AABB;
        Near((Bounds.Min-Original.Min).Length(),0,"Split model minimum bounds match original",0.0001);
        Near((Bounds.Max-Original.Max).Length(),0,"Split model maximum bounds match original",0.0001);
        Require(Indices == Source->GetMeshData()->Indices.Num(), "Every original triangle is displayed exactly once");
        const FVector Forward = Car->GetActorRotation().Quaternion().GetForwardVector();
        Require((Camera->GetWorldLocation()-Car->GetActorLocation()).Dot(Forward)<-5, "Camera stays behind the selected car at every yaw");
        Require(Camera->GetWorldRotation().Quaternion().GetForwardVector().Dot(Forward)>.9f, "Camera looks in the driving direction");
    }
    Car->SetActorRotation(FRotator()); Car->SetBody("ghini");
    auto States = World.GetPhysicsScene().CapturePhysicsInputs();
    for (auto& State : States) if (World.GetPhysicsScene().FindPrimitive(State.BodyId) == Body) State.LinearVelocity = FVector(5,0,0);
    World.GetPhysicsScene().GetSolver().SetBodyStates(States);
    const float StartX = Car->GetActorLocation().X;
    for (int I=0; I<30; ++I) World.Tick(EWorldTick::All,1.0f/60.0f);
    const FBox Local = Wheels[0]->GetMeshData()->AABB;
    const float Radius = (Local.Max.Z-Local.Min.Z)*0.5f*Wheels[0]->GetWorldScale3D().Z;
    const FQuat Expected = FQuat::MakeFromAxisAngle(FVector(0,1,0),(Car->GetActorLocation().X-StartX)/Radius);
    Near(Wheels[0]->GetRelativeRotationQuat().RotateVector(FVector(1,0,0)).Dot(Expected.RotateVector(FVector(1,0,0))),1,"Wheel roll follows actual Solver travel and scale",0.0001);
    const FQuat BeforeReverse = Wheels[0]->GetRelativeRotationQuat();
    const float ReverseStartX = Car->GetActorLocation().X;
    Body->UnregisterComponent(); Body->RegisterComponentWithWorld(&World);
    States = World.GetPhysicsScene().CapturePhysicsInputs();
    for (auto& State : States) if (World.GetPhysicsScene().FindPrimitive(State.BodyId) == Body) State.LinearVelocity = FVector(-5,0,0);
    World.GetPhysicsScene().GetSolver().SetBodyStates(States);
    for (int I=0; I<30; ++I) World.Tick(EWorldTick::All,1.0f/60.0f);
    Require(Car->GetActorLocation().X < ReverseStartX,"Vehicle reverses in Solver");
    const FQuat ReverseExpected = BeforeReverse * FQuat::MakeFromAxisAngle(FVector(0,1,0),(Car->GetActorLocation().X-ReverseStartX)/Radius);
    Near(Wheels[0]->GetRelativeRotationQuat().RotateVector(FVector(1,0,0)).Dot(ReverseExpected.RotateVector(FVector(1,0,0))),1,"Reverse travel rolls the wheel backwards",0.0001);
    for (int I=0; I<60; ++I) { Car->GetMovement()->SetInput(0,1,false); World.Tick(EWorldTick::All,1.0f/60.0f); }
    for (int I=0; I<4; ++I)
    {
        const FQuat Steer = FQuat::MakeFromAxisAngle(FVector(0,0,1),I<2?FMath::DegreesToRadians(25):0);
        Near(Wheels[I]->GetRelativeRotationQuat().RotateVector(FVector(0,1,0)).Dot(Steer.RotateVector(FVector(0,1,0))),1,"Only front wheel axes steer",0.0001);
    }
    for (int I=0; I<60; ++I) World.Tick(EWorldTick::All,1.0f/60.0f);
    Near(Wheels[0]->GetRelativeRotationQuat().RotateVector(FVector(0,1,0)).Y,1,"Releasing steering centers the front wheels",0.0001);
    World.EndPlay();
    Require(Car->GetBodyMeshComponent()->IsVisible() && !Wheels[0]->IsVisible(),"Stopping restores the editable original model");
    World.ClearWorld();
    std::cout << "PASS: fresh OBJ cache, all 12 car selections, shared parts, geometry/material preservation, save/load, scaled wheel roll, front steering and rear chase camera\n";
}

int main(int argc, char** argv)
{
    try
    {
        if (argc > 1 && FString(argv[1]) == "--vehicle-visuals") { VehicleVisualCases(); return 0; }
        FFixture Fixture;
        FixedCases(Fixture);
        RandomCases(Fixture);
        std::cout << "PASS: BC/CC closest distance, face/edge/endpoints, containment/crossing, degeneracy, scale/rotation, 2000 oracle comparisons and Solver separation\n";
        return 0;
    }
    catch (const std::exception& Error)
    {
        std::cerr << "FAIL: " << Error.what() << '\n';
        return 1;
    }
}
