#include "SimAimRig.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"

void FSimAimRig::Initialize(AActor* Robot)
{
    if (!Robot || !Robot->GetRootComponent()) return;
    auto* PitchPivot = NewObject<USceneComponent>(Robot, TEXT("BarrelPitchPivot"));
    PitchPivot->SetupAttachment(Robot->GetRootComponent());
    PitchPivot->RegisterComponent();
    PitchPivot->SetRelativeLocation(FVector(5.0f, 0.0f, 8.0f));
    Pivot = PitchPivot;

    auto* CameraPoint = NewObject<USceneComponent>(Robot, TEXT("CameraAnchor"));
    CameraPoint->SetupAttachment(PitchPivot);
    CameraPoint->RegisterComponent();
    CameraPoint->SetRelativeLocation(FVector(-5.0f, 0.0f, 8.0f));
    Camera = CameraPoint;

    auto* MuzzlePoint = NewObject<USceneComponent>(Robot, TEXT("MuzzleAnchor"));
    MuzzlePoint->SetupAttachment(PitchPivot);
    MuzzlePoint->RegisterComponent();
    MuzzlePoint->SetRelativeLocation(FVector(12.0f, 0.0f, 0.0f));
    Muzzle = MuzzlePoint;

    auto* Tube = NewObject<UStaticMeshComponent>(Robot, TEXT("BarrelVisual"));
    Tube->SetupAttachment(PitchPivot);
    Tube->SetStaticMesh(LoadObject<UStaticMesh>(
        nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
    Tube->RegisterComponent();
    Tube->SetRelativeLocation(FVector(6.0f, 0.0f, 0.0f));
    Tube->SetRelativeScale3D(FVector(0.12f, 0.035f, 0.035f));
    Tube->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    if (auto* Base = LoadObject<UMaterialInterface>(
            nullptr, TEXT("/Game/Generated/M_Robot.M_Robot"))) {
        auto* Instance = UMaterialInstanceDynamic::Create(Base, Robot);
        Instance->SetVectorParameterValue(TEXT("Color"),
                                          FLinearColor(0.18f, 0.22f, 0.28f));
        Tube->SetMaterial(0, Instance);
    }
    CurrentPitch = 0.0f;
}

void FSimAimRig::AdvancePitch(double Input, double Dt, double Sensitivity)
{
    if (!Pivot.IsValid()) return;
    const float Increment = static_cast<float>(
        FMath::Clamp(Input, -1.0, 1.0) * 95.0 * Sensitivity * Dt);
    CurrentPitch = FMath::Clamp(CurrentPitch + Increment, -10.0f, 25.0f);
    Pivot->SetRelativeRotation(FRotator(CurrentPitch, 0.0f, 0.0f));
}
