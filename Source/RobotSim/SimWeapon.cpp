#include "SimWeapon.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

bool FSimWeapon::TryFire(UWorld* World, AActor* Robot, double SimTime, bool bTrigger)
{
    if (!World || !Robot || !bTrigger || SimTime + 1e-9 < NextFireTime)
        return false;
    NextFireTime = SimTime + 0.25; // 試験用の4発/秒。実機の射出特性ではない。
    ++Count;
    const FVector Start = Robot->GetActorLocation() +
        Robot->GetActorForwardVector() * 45.0f + FVector(0, 0, 20.0f);
    const FVector End = Start + Robot->GetActorForwardVector() * 2500.0f;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(SimFire), false, Robot);
    FHitResult Hit;
    const bool bHit = World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Query);
    const FVector Destination = bHit ? Hit.ImpactPoint : End;
    DrawDebugLine(World, Start, Destination, FColor::Orange, false, 0.12f, 0, 2.0f);
    if (bHit)
        DrawDebugSphere(World, Destination, 5.0f, 8, FColor::Yellow, false, 0.16f);
    UE_LOG(LogTemp, Display, TEXT("SIM_FIRE: 時刻=%.3f 発射数=%d 衝突=%s"),
           SimTime, Count, bHit ? TEXT("はい") : TEXT("いいえ"));
    return true;
}
