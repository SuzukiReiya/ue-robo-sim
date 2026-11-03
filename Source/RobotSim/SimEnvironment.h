#pragma once
#include "CoreMinimal.h"
class AActor;
class UWorld;

// 屋内・フィールドの生成だけを担当。操作・機体運動・撮影とは独立。
class FSimEnvironment
{
public:
    static void Build(UWorld* World, AActor*& OutRobot, AActor*& OutTarget,
                      AActor*& OutSpinningTarget);
};
