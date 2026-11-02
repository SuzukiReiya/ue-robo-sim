#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SimVideoPipeline.generated.h"

class AActor;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;

// カメラ取得→映像遅延→操縦者表示を一括管理する独立モジュール。
// 制御ルータや機体プラントへは参照を持たない。
UCLASS()
class ROBOTSIM_API URobotVideoPipeline : public UObject
{
    GENERATED_BODY()
public:
    void Initialize(AActor* CameraRobot, bool bEnableExport);
    void Advance(double SimTime, float VideoDelayMs, float JitterMs,
                 float LossRate, FRandomStream& Random);
    UTextureRenderTarget2D* DisplayTexture() const;
    double DisplayAge(double SimTime) const;
private:
    void Capture(double SimTime, float VideoDelayMs, float JitterMs,
                 float LossRate, FRandomStream& Random);
    void ExportFrame(UTextureRenderTarget2D* Texture, uint64 Id, double ShotTime);
    UPROPERTY() TObjectPtr<USceneCaptureComponent2D> Sensor;
    UPROPERTY() TArray<TObjectPtr<UTextureRenderTarget2D>> Frames;
    struct FPendingFrame { int32 Index; double Due; double Shot; };
    TArray<FPendingFrame> Pending;
    int32 DisplayIndex = INDEX_NONE, NextIndex = 0;
    uint64 FrameId = 0;
    double DisplayShot = 0.0, NextCapture = 0.0;
    bool bExport = false;
};
