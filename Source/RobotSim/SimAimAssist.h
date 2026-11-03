#pragma once
#include "CoreMinimal.h"
#include "SimAimMath.h"

class UWorld;
class AActor;

// 補正候補の取得層。世界座標を使うデモ実装であり、画像認識器ではない。
// 将来ここをカメラ画像から検出・追跡した候補に差し替える。
class FSimAimAssist
{
public:
    static RobotSim::Aim::Selection Find(
        UWorld* World, const FTransform& Camera, const FVector& MuzzleLocation,
        float HorizontalFov, AActor* Viewer,
        AActor* FirstTarget, AActor* SecondTarget);
};
