#include "SimAimAssist.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include <vector>

namespace {
RobotSim::Aim::Vec3 ToSimple(const FVector& P) {
    return {P.X, P.Y, P.Z};
}
}

RobotSim::Aim::Selection FSimAimAssist::Find(
    UWorld* World, const FTransform& Camera, const FVector& MuzzleLocation,
    float HorizontalFov, AActor* Viewer,
    AActor* FirstTarget, AActor* SecondTarget)
{
    if (!World || !Viewer) return {};
    std::vector<RobotSim::Aim::Candidate> Candidates;
    const AActor* Enemies[] = {FirstTarget, SecondTarget};
    for (int32 TargetIndex=0; TargetIndex<2; ++TargetIndex) {
        auto* Enemy = const_cast<AActor*>(Enemies[TargetIndex]);
        if (!Enemy) continue;
        TArray<UBoxComponent*> Sensors;
        Enemy->GetComponents<UBoxComponent>(Sensors);
        int32 PanelIndex = 0;
        for (auto* Sensor : Sensors) {
            if (!Sensor || !Sensor->ComponentHasTag(FName(TEXT("HitPanel"))))
                continue;
            const FVector End = Sensor->GetComponentLocation();
            FHitResult Hit;
            FCollisionQueryParams Params(SCENE_QUERY_STAT(SimAimVisibility), false);
            Params.AddIgnoredActor(Viewer);
            const bool bHit = World->LineTraceSingleByChannel(
                Hit, Camera.GetLocation(), End, ECC_Visibility, Params);
            const bool bVisible = bHit && Hit.GetComponent() == Sensor;
            Candidates.push_back({
                TargetIndex * 4 + PanelIndex,
                ToSimple(End), bVisible
            });
            ++PanelIndex;
        }
    }
    const FRotator Angle = Camera.GetRotation().Rotator();
    return RobotSim::Aim::select(
        ToSimple(Camera.GetLocation()), Angle.Yaw, Angle.Pitch,
        ToSimple(MuzzleLocation), Candidates,
        HorizontalFov, 640.0/360.0, 600.0, 0.35);
}
