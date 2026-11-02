#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SimControl.h"
#include "SimVehicle.h"
#include "Serialization/Archive.h"
#include "SimGameMode.generated.h"

class URobotVideoPipeline;
class UTextureRenderTarget2D;
class FSocket;
class FArchive;

// ループの時刻管理と各独立モジュールの接続だけを担当する。
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
    void Step(double Dt);
    void ReceiveExternal();
    void SwitchMode(RobotSim::Mode NewMode);
    UPROPERTY() TObjectPtr<AActor> Robot;
    UPROPERTY() TObjectPtr<AActor> Target;
    UPROPERTY() TObjectPtr<URobotVideoPipeline> Video;
    RobotSim::CommandRouter Controller;
    FSimVehicle Vehicle;
    double SimTime = 0, Accumulator = 0, NextInput = 0, NextLog = 0;
    float CommandDelayMs = 100, VideoDelayMs = 100, SharedWeight = 0.5f;
    float JitterMs = 0, LossRate = 0;
    bool bExport = false, bDebug = false;
    int32 Seed = 12345;
    FRandomStream Random;
    FSocket* Socket = nullptr;
    TUniquePtr<FArchive> Log;
    FString RunDirectory;
};

