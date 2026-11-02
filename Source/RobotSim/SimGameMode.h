#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "SimCore.h"
#include "Serialization/Archive.h"
#include "SimGameMode.generated.h"

class UTextureRenderTarget2D;
class USceneCaptureComponent2D;
class FSocket;
class FArchive;

UCLASS()
class ROBOTSIM_API ASimGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ASimGameMode();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UTextureRenderTarget2D* DisplayTexture() const;
    FString Status() const;
private:
    AActor* MakeBox(FVector Location, FVector Size, FLinearColor Color);
    void Step(double Dt);
    void ReceiveExternal();
    void CaptureFrame();
    void ExportFrame(UTextureRenderTarget2D* Texture, uint64 Id, double ShotTime);
    void SwitchMode(int32 NewMode);
    UPROPERTY() TObjectPtr<AActor> Robot;
    UPROPERTY() TObjectPtr<AActor> Target;
    UPROPERTY() TObjectPtr<USceneCaptureComponent2D> Sensor;
    UPROPERTY() TArray<TObjectPtr<UTextureRenderTarget2D>> Frames;
    struct PendingFrame { int32 Index; double Due; double Shot; uint64 Id; };
    TArray<PendingFrame> Pending;
    int32 DisplayIndex=-1, NextIndex=0, Mode=0;
    uint64 FrameId=0;
    double SimTime=0, Accumulator=0, NextInput=0, NextCapture=0, LastExternal=-100;
    double LastSequence=-1, DisplayShot=0, NextLog=0, LastManual=0;
    float CommandDelayMs=100, VideoDelayMs=100, SharedWeight=0.5f;
    float JitterMs=0, LossRate=0;
    bool bExport=false, bDebug=false;
    int32 Seed=12345;
    FRandomStream Random;
    RobotSim::DelayQueue<RobotSim::Command> Commands;
    RobotSim::Command Manual, External, Applied;
    RobotSim::Drive Drive;
    FSocket* Socket=nullptr;
    TUniquePtr<FArchive> Log;
    FString RunDirectory;
};

UCLASS()
class ROBOTSIM_API ASimHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
};
