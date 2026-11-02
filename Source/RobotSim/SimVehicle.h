#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SimCore.h"

// 物理・運動モデルのUE適用部。指令の生成やカメラ表示には依存しない。
// 実車両モデルへ変更する場合は本クラスを差し替える。
class FSimVehicle
{
public:
    void Initialize(AActor* InRobot) { Robot = InRobot; Motion = {}; bBlocked = false; }
    void ResetMotion() { Motion = {}; bBlocked = false; }
    void Advance(RobotSim::Command Command, double Dt)
    {
        if (!Robot.IsValid()) return;
        Motion.step(Command, Dt, 150.0, 120.0, 0.15);
        const double Yaw = Robot->GetActorRotation().Yaw + Motion.yawRate * Dt * 0.5;
        const double Angle = FMath::DegreesToRadians(Yaw);
        FHitResult Hit;
        Robot->AddActorWorldOffset(
            FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0) * Motion.speed * Dt,
            true, &Hit);
        bBlocked = Hit.bBlockingHit;
        if (bBlocked) Motion.speed = 0;
        // 初期モデルでは旋回時の形状Sweepは行わない。
        Robot->AddActorWorldRotation(FRotator(0, Motion.yawRate * Dt, 0));
    }
    double Speed() const { return Motion.speed; }
    double YawRate() const { return Motion.yawRate; }
    bool IsBlocked() const { return bBlocked; }
private:
    TWeakObjectPtr<AActor> Robot;
    RobotSim::Drive Motion;
    bool bBlocked = false;
};
