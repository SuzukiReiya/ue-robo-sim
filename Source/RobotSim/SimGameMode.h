#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SimControl.h"
#include "SimVehicle.h"
#include "SimInputBindings.h"
#include "SimWeapon.h"
#include "Serialization/Archive.h"
#include "SimGameMode.generated.h"

class URobotVideoPipeline;
class USimSettingsWidget;
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
    void SetSimulationPaused(bool bPause);
    bool ApplySettings(const RobotSim::InputBindings& NewBindings,
                       const RobotSim::Tuning& NewTuning);
private:
    void Step(double Dt);
    void ReceiveExternal();
    void SwitchMode(RobotSim::Mode NewMode);
    void LoadInputBindings();
    void FlushExternal();
    FString BindingsPath() const;
    UPROPERTY() TObjectPtr<AActor> Robot;
    UPROPERTY() TObjectPtr<AActor> Target;
    UPROPERTY() TObjectPtr<URobotVideoPipeline> Video;
    UPROPERTY() TObjectPtr<USimSettingsWidget> SettingsWidget;
    RobotSim::CommandRouter Controller;
    FSimVehicle Vehicle;
    FSimWeapon Weapon;
    RobotSim::InputBindings Bindings;
    RobotSim::Tuning Tuning;
    RobotSim::RawInput RawInput;
    RobotSim::OperatorActions Actions;
    double SimTime = 0, Accumulator = 0, NextInput = 0, NextLog = 0;
    float CommandDelayMs = 100, VideoDelayMs = 100, SharedWeight = 0.5f;
    float JitterMs = 0, LossRate = 0;
    bool bExport = false, bDebug = false, bPaused = false, bAwaitEscapeRelease = false, bAwaitMouseRelease = false, bSkipMouseFrame = false;
    int32 Seed = 12345;
    FRandomStream Random;
    FRandomStream WeaponRandom;
    FSocket* Socket = nullptr;
    TUniquePtr<FArchive> Log;
    FString RunDirectory;
};

