#include "SimWeapon.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"

void FSimWeapon::Advance(double Dt)
{
    // 次の発射までに反動が収束する簡易モデル。
    const double A = FMath::Exp(-Dt / 0.18);
    Recoil = Recoil * A;
}

bool FSimWeapon::TryFire(UWorld* World, AActor* Robot, double SimTime, bool bTrigger,
                         const FRotator& BodySway, double RecoilDegrees,
                         FRandomStream& Random)
{
    if (!World || !Robot || !Muzzle.IsValid() || !bTrigger ||
        SimTime + 1e-9 < NextFireTime)
        return false;
    NextFireTime = SimTime + 0.25;
    ++Count;
    const double DispersionPitch = Random.FRandRange(-RecoilDegrees, RecoilDegrees);
    const double DispersionYaw = Random.FRandRange(-RecoilDegrees, RecoilDegrees);
    const FRotator Offset = BodySway + Recoil +
        FRotator(DispersionPitch, DispersionYaw, 0);
    // 独立した砲口の座標と角度を使用。カメラ中心からは発射しない。
    const FVector Start = Muzzle->GetComponentLocation();
    const FVector Direction = (Muzzle->GetComponentRotation() + Offset).Vector();
    const FVector End = Start + Direction * 2500.0f;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(SimFire), false, Robot);
    FHitResult Hit;
    const bool bHit = World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Query);
    DrawDebugLine(World, Start, bHit ? Hit.ImpactPoint : End, FColor::Orange, false, 0.12f, 0, 2.0f);
    const bool bPanelHit = bHit && Hit.GetComponent() &&
        Hit.GetComponent()->ComponentHasTag(FName(TEXT("HitPanel")));
    if (bHit)
        DrawDebugSphere(World, Hit.ImpactPoint, 5.0f, 8,
                        bPanelHit ? FColor::Green : FColor::Yellow, false, 0.16f);
    if (bPanelHit) ++PanelHits;
    // キックバックは次フレーム以降のカメラ姿勢と射線に残る。
    Recoil.Pitch += RecoilDegrees * 0.6;
    Recoil.Yaw += DispersionYaw * 0.5;
    UE_LOG(LogTemp, Display, TEXT("SIM_FIRE: 時刻=%.3f 発射数=%d 衝突=%s パネル被弾=%s 累計=%d"),
           SimTime, Count, bHit ? TEXT("はい") : TEXT("いいえ"),
           bPanelHit ? TEXT("はい") : TEXT("いいえ"), PanelHits);
    return true;
}
