#pragma once
#include "CoreMinimal.h"
class AActor;
class UWorld;

// 射撃は入力経路から独立。暫定的に前方へのレイキャストで発射を可視化する。
class FSimWeapon
{
public:
    bool TryFire(UWorld* World, AActor* Robot, double SimTime, bool bTrigger);
    int32 FiredCount() const { return Count; }
private:
    double NextFireTime = 0;
    int32 Count = 0;
};
