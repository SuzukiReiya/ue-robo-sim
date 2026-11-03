#pragma once
#include "CoreMinimal.h"
class AActor;
class USceneComponent;

// 自機の非スケール座標系に取り付けた共通ピッチ軸。
// カメラは砲口より8cm上／約17cm後方。砲口位置から射線を発生させる。
class FSimAimRig
{
public:
    void Initialize(AActor* Robot);
    void AdvancePitch(double Input, double Dt, double Sensitivity);
    float PitchDegrees() const { return CurrentPitch; }
    USceneComponent* CameraMount() const { return Camera.Get(); }
    USceneComponent* MuzzleMount() const { return Muzzle.Get(); }
private:
    TWeakObjectPtr<USceneComponent> Pivot, Camera, Muzzle;
    float CurrentPitch = 0.0f;
};
