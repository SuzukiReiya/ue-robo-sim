#pragma once
#include "CoreMinimal.h"
class AActor;
class UWorld;
class USceneComponent;

// 射撃反動の姿勢ずれはカメラと射線へ共通適用する。射撃入力・GUIは参照しない。
class FSimWeapon
{
public:
    void Initialize(USceneComponent* InMuzzle) { Muzzle = InMuzzle; }
    void Advance(double Dt);
    bool TryFire(UWorld* World, AActor* Robot, double SimTime, bool bTrigger,
                 const FRotator& BodySway, double RecoilDegrees, FRandomStream& Random);
    FRotator RecoilOffset() const { return Recoil; }
    int32 FiredCount() const { return Count; }
    int32 PanelHitCount() const { return PanelHits; }
private:
    TWeakObjectPtr<USceneComponent> Muzzle;
    double NextFireTime = 0;
    int32 Count = 0;
    int32 PanelHits = 0;
    FRotator Recoil = FRotator::ZeroRotator;
};
