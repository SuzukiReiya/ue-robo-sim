#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SimCore.h"
#include "SimTuning.h"

// 運動・滑り・慣性揺れのプラント。カメラや入力デバイスへの参照を持たない。
class FSimVehicle
{
public:
    void Initialize(AActor* InRobot) { Robot = InRobot; ResetMotion(); }
    void ResetMotion() {
        Motion = {}; WorldVelocity = FVector::ZeroVector;
        PreviousVelocity = FVector::ZeroVector;
        Sway = FRotator::ZeroRotator;
        bBlocked = false;
    }
    void Advance(RobotSim::Command Command, double Dt, const RobotSim::Tuning& Config)
    {
        if (!Robot.IsValid()) return;
        // 操舵入力解除後もYaw角速度を時定数に従って減衰させる。
        // 走行の滑りと旋回の慣性は別パラメータで独立評価できる。
        Motion.step(Command, Dt, 150.0, 120.0, 0.15,
                    FMath::Max(0.001, Config.turnInertiaMs / 1000.0));
        const double Yaw = Robot->GetActorRotation().Yaw + Motion.yawRate * Dt * 0.5;
        const double Angle = FMath::DegreesToRadians(Yaw);
        const FVector Forward(FMath::Cos(Angle), FMath::Sin(Angle), 0);
        const FVector Right(-FMath::Sin(Angle), FMath::Cos(Angle), 0);
        const FVector Desired = Forward * Motion.speed + Right * Motion.lateralSpeed;
        // グリップが低いと世界座標系の慣性速度を保ち、旋回しても横滑りする。
        const double Response = 0.035 + 0.75 * Config.tireSlip;
        const double Alpha = 1.0 - FMath::Exp(-Dt / Response);
        WorldVelocity += (Desired - WorldVelocity) * Alpha;
        FHitResult Hit;
        Robot->AddActorWorldOffset(WorldVelocity * Dt, true, &Hit);
        bBlocked = Hit.bBlockingHit;
        if (bBlocked) {
            Motion.speed = 0;
            Motion.lateralSpeed = 0;
            WorldVelocity = FVector::ZeroVector;
        }
        Robot->AddActorWorldRotation(FRotator(0, Motion.yawRate * Dt, 0));
        // 加減速と旋回の慣性をカメラと射撃に共通の姿勢揺れとして適用。
        const FVector Acceleration = (WorldVelocity - PreviousVelocity) / FMath::Max(Dt, 0.0001);
        PreviousVelocity = WorldVelocity;
        const double Longitudinal = FVector::DotProduct(Acceleration, Forward);
        const double Lateral = FVector::DotProduct(Acceleration, Right);
        const double MaxTilt = Config.bodySwayDegrees;
        const FRotator TargetTilt(
            FMath::Clamp(-Longitudinal * 0.005, -1.0, 1.0) * MaxTilt,
            FMath::Clamp(-Motion.yawRate * 0.002, -0.5, 0.5) * MaxTilt,
            FMath::Clamp(Lateral * 0.005, -1.0, 1.0) * MaxTilt);
        const double Settle = 1.0 - FMath::Exp(-Dt / 0.22);
        Sway = FMath::Lerp(Sway, TargetTilt, Settle);
    }
    double Speed() const { return Motion.speed; }
    double LateralSpeed() const { return Motion.lateralSpeed; }
    double YawRate() const { return Motion.yawRate; }
    bool IsBlocked() const { return bBlocked; }
    FRotator BodySway() const { return Sway; }
private:
    TWeakObjectPtr<AActor> Robot;
    RobotSim::Drive Motion;
    FVector WorldVelocity = FVector::ZeroVector;
    FVector PreviousVelocity = FVector::ZeroVector;
    FRotator Sway = FRotator::ZeroRotator;
    bool bBlocked = false;
};
