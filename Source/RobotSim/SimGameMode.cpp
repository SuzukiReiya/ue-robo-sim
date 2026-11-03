#include "SimGameMode.h"
#include "SimHUD.h"
#include "SimEnvironment.h"
#include "SimKeyboardInput.h"
#include "SimSettingsWidget.h"
#include "Blueprint/UserWidget.h"
#include "SimVideoPipeline.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/LightComponent.h"
#include "Engine/World.h"
#include "Engine/DirectionalLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Canvas.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Common/UdpSocketBuilder.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/Archive.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"

ASimGameMode::ASimGameMode()
{
    PrimaryActorTick.bCanEverTick=true;
    DefaultPawnClass=nullptr;
    HUDClass=ASimHUD::StaticClass();
}
void ASimGameMode::BeginPlay()
{
    Super::BeginPlay();
    FParse::Value(FCommandLine::Get(),TEXT("CommandDelayMs="),CommandDelayMs);
    FParse::Value(FCommandLine::Get(),TEXT("VideoDelayMs="),VideoDelayMs);
    FParse::Value(FCommandLine::Get(),TEXT("JitterMs="),JitterMs);
    FParse::Value(FCommandLine::Get(),TEXT("LossRate="),LossRate);
    FParse::Value(FCommandLine::Get(),TEXT("SharedWeight="),SharedWeight);
    FParse::Value(FCommandLine::Get(),TEXT("Seed="),Seed);
    // 非有限値を含む起動設定は既定値へ戻す。
    auto Sanitize=[](float V,float Fallback,float Low,float High) {
        return FMath::IsFinite(V)?FMath::Clamp(V,Low,High):Fallback;
    };
    CommandDelayMs=Sanitize(CommandDelayMs,100,0,1000);
    VideoDelayMs=Sanitize(VideoDelayMs,100,0,1000);
    JitterMs=Sanitize(JitterMs,0,0,200);
    LossRate=Sanitize(LossRate,0,0,1);
    SharedWeight=Sanitize(SharedWeight,0.5f,0,1);
    Random.Initialize(Seed);
    LoadInputBindings();
    bExport=FParse::Param(FCommandLine::Get(),TEXT("ExportFrames"));
    // 独立した環境・プラント・映像モジュールを初期化する。
    AActor* SpawnedRobot = nullptr;
    AActor* SpawnedTarget = nullptr;
    FSimEnvironment::Build(GetWorld(), SpawnedRobot, SpawnedTarget);
    Robot = SpawnedRobot;
    Target = SpawnedTarget;
    Vehicle.Initialize(Robot);
    Video = NewObject<URobotVideoPipeline>(this);
    Video->Initialize(Robot, bExport);
    Socket=FUdpSocketBuilder(TEXT("RobotSimCommand")).AsNonBlocking()
        .BoundToAddress(FIPv4Address(127,0,0,1)).BoundToPort(9876).WithReceiveBufferSize(65536);
    if(!Socket) UE_LOG(LogTemp,Error,TEXT("外部指令ポート9876を開けません。別のシミュレータが起動していないか確認してください。"));
    RunDirectory=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Experiments"),
        FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%S"))+TEXT("_")+FGuid::NewGuid().ToString());
    IFileManager::Get().MakeDirectory(*RunDirectory,true);
    Log.Reset(IFileManager::Get().CreateFileWriter(*FPaths::Combine(RunDirectory,TEXT("truth.csv"))));
    const FString Header=TEXT("sim_s,x_cm,y_cm,yaw_deg,speed_cm_s,yaw_deg_s,manual_throttle,manual_turn,external_throttle,external_turn,applied_throttle,applied_turn,mode,display_age_s\n");
    if(Log) { FTCHARToUTF8 Bytes(*Header); Log->Serialize((void*)Bytes.Get(),Bytes.Length()); }
    const FString Settings=FString::Printf(TEXT("{\"seed\":%d,\"command_delay_ms\":%g,\"video_delay_ms\":%g,\"jitter_ms\":%g,\"loss_rate\":%g,\"shared_weight\":%g,\"export_frames\":%s,\"physics_hz\":120,\"camera_hz\":20}"),Seed,CommandDelayMs,VideoDelayMs,JitterMs,LossRate,SharedWeight,bExport?TEXT("true"):TEXT("false"));
    FFileHelper::SaveStringToFile(Settings,*FPaths::Combine(RunDirectory,TEXT("settings.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    if(auto* PC=GetWorld()->GetFirstPlayerController()) { PC->bShowMouseCursor=false; PC->SetInputMode(FInputModeGameOnly()); }
    UE_LOG(LogTemp,Display,TEXT("ROBOT_SIM_READY: %s"),*RunDirectory);
}
void ASimGameMode::SwitchMode(RobotSim::Mode NewMode)
{
    Controller.selectMode(NewMode);
}
void ASimGameMode::ReceiveExternal()
{
    if(!Socket) return;
    uint32 PendingSize=0;
    // 受信洪水でフレームを占有しない。
    for(int32 Count=0;Count<64 && Socket->HasPendingData(PendingSize);++Count) {
        uint8 Buffer[2048]; int32 Read=0;
        if(!Socket->Recv(Buffer,sizeof(Buffer)-1,Read) || Read<=0) break;
        Buffer[Read]=0;
        TSharedPtr<FJsonObject> Object;
        auto Reader=TJsonReaderFactory<>::Create(FString(UTF8_TO_TCHAR(reinterpret_cast<char*>(Buffer))));
        if(!FJsonSerializer::Deserialize(Reader,Object)||!Object.IsValid()) continue;
        double Seq,T,R;
        if(!Object->TryGetNumberField(TEXT("seq"),Seq)||!Object->TryGetNumberField(TEXT("throttle"),T)||!Object->TryGetNumberField(TEXT("turn"),R)) continue;
        Controller.submitExternal(SimTime, Seq, {T, R});
    }
}
void ASimGameMode::Step(double Dt)
{
    SimTime += Dt;
    const APlayerController* PC = GetWorld()->GetFirstPlayerController();
    // 生入力はTickで一度だけ取得。機体指令と射撃は個別の出口へ振り分ける。
    if (SimTime >= NextInput) {
        NextInput = SimTime + 0.02;
        if (Random.FRand() >= LossRate) {
            const double Delay = FMath::Max(
                0.0f, CommandDelayMs + Random.FRandRange(-JitterMs, JitterMs)) / 1000.0;
            Actions = RobotSim::mapInput(Bindings, RawInput);
            Controller.submitCommand(Actions.movement, SimTime, Delay);
        }
    }
    RobotSim::Command Applied = Controller.resolve(SimTime, SharedWeight);
    // 緊急停止だけは通常の指令遅延・モード裁定を通さない。
    if (FSimKeyboardInput::EmergencyStop(PC)) {
        Controller.emergencyStop();
        Applied = {};
        Vehicle.ResetMotion();
        Actions.fire = false;
    }
    if (Controller.mode() != RobotSim::Mode::Automatic &&
        !FSimKeyboardInput::EmergencyStop(PC))
        Weapon.TryFire(GetWorld(), Robot, SimTime, Actions.fire);
    Vehicle.Advance(Applied, Dt);
    Target->SetActorLocation(FVector(400, 300 * std::sin(SimTime * 0.5), 25));
    if (Log && SimTime >= NextLog) {
        NextLog = SimTime + 0.02;
        const FVector P = Robot->GetActorLocation();
        const RobotSim::Command Manual = Controller.manual();
        const RobotSim::Command External = Controller.freshExternal(SimTime);
        const FString Line = FString::Printf(
            TEXT("%.6f,%.4f,%.4f,%.4f,%.4f,%.4f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%d,%.6f\n"),
            SimTime, P.X, P.Y, Robot->GetActorRotation().Yaw,
            Vehicle.Speed(), Vehicle.YawRate(),
            Manual.throttle, Manual.turn, External.throttle, External.turn,
            Applied.throttle, Applied.turn, static_cast<int32>(Controller.mode()),
            Video ? Video->DisplayAge(SimTime) : -1.0);
        FTCHARToUTF8 Bytes(*Line);
        Log->Serialize((void*)Bytes.Get(), Bytes.Length());
    }
}
void ASimGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if(auto* PC=GetWorld()->GetFirstPlayerController()) {
        if (!PC->IsInputKeyDown(EKeys::Escape)) bAwaitEscapeRelease = false;
        if(PC->WasInputKeyJustPressed(EKeys::One)) SwitchMode(RobotSim::Mode::Manual);
        if(PC->WasInputKeyJustPressed(EKeys::Two)) SwitchMode(RobotSim::Mode::Shared);
        if(PC->WasInputKeyJustPressed(EKeys::Three)) SwitchMode(RobotSim::Mode::Automatic);
        if(PC->WasInputKeyJustPressed(EKeys::F1)) bDebug=!bDebug;
        if (!bPaused && !bAwaitEscapeRelease && PC->WasInputKeyJustPressed(EKeys::Escape)) {
            SetSimulationPaused(true);
            return;
        }
    }
    // メニュー中はシミュレーション時間、機体、映像、対象、指令経路を停止する。
    if (bPaused) return;
    RawInput = FSimKeyboardInput::Read(GetWorld()->GetFirstPlayerController());
    ReceiveExternal();
    // 重いフレームでは追い付ける分だけ進め、破棄時間をログに明示。
    if(DeltaSeconds>0.25f) UE_LOG(LogTemp,Warning,TEXT("SIM_OVERRUN: %.6f秒を破棄。実時間評価からこの試行を除外してください。"),DeltaSeconds-0.25f);
    Accumulator+=FMath::Min(DeltaSeconds,0.25f);
    constexpr double Dt=1.0/120.0;
    while(Accumulator>=Dt) { Step(Dt); Accumulator-=Dt; }
    if (Video) Video->Advance(SimTime, VideoDelayMs, JitterMs, LossRate, Random);
}
UTextureRenderTarget2D* ASimGameMode::DisplayTexture() const
{
    return Video ? Video->DisplayTexture() : nullptr;
}
FString ASimGameMode::Status() const
{
    if (!bDebug) return FString();
    const FVector Position = Robot ? Robot->GetActorLocation() : FVector::ZeroVector;
    return FString::Printf(
        TEXT("1:手動 2:共有 3:自動 | モード=%d | W/S:前後 A/D:旋回 | Space:非常停止\n")
        TEXT("前後入力(手動/適用)=%+.2f/%+.2f | 速度=%+.1fcm/s | 位置=(%.0f, %.0f)cm | 移動遮断=%s\n")
        TEXT("射撃回数=%d | 指令 %.0fms 映像 %.0fms | 画像経過 %.0fms | Esc:設定"),
        static_cast<int32>(Controller.mode()),
        Controller.manual().throttle, Controller.applied().throttle,
        Vehicle.Speed(), Position.X, Position.Y,
        Vehicle.IsBlocked() ? TEXT("はい") : TEXT("いいえ"),
        Weapon.FiredCount(), CommandDelayMs, VideoDelayMs,
        Video ? Video->DisplayAge(SimTime) * 1000 : -1.0);
}

FString ASimGameMode::BindingsPath() const
{
    return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Config"),
                           TEXT("InputBindings.json"));
}

void ASimGameMode::LoadInputBindings()
{
    Bindings = {};
    FString Json;
    if (!FFileHelper::LoadFileToString(Json, *BindingsPath())) return;
    TSharedPtr<FJsonObject> Obj;
    const auto Reader = TJsonReaderFactory<>::Create(Json);
    if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid()) {
        UE_LOG(LogTemp, Warning, TEXT("操作設定を読めません。初期値を使用します。"));
        return;
    }
    RobotSim::InputBindings Loaded;
    const TCHAR* Fields[] = {
        TEXT("forward"), TEXT("backward"), TEXT("turn_left"),
        TEXT("turn_right"), TEXT("fire")
    };
    RobotSim::InputSource* Entries[] = {
        &Loaded.forward, &Loaded.backward, &Loaded.turnLeft,
        &Loaded.turnRight, &Loaded.fire
    };
    for (int32 I=0; I<5; ++I) {
        FString Token;
        if (!Obj->TryGetStringField(Fields[I], Token) ||
            !RobotSim::parseSource(TCHAR_TO_UTF8(*Token), *Entries[I])) {
            UE_LOG(LogTemp, Warning, TEXT("操作設定の項目が無効です。初期値に戻します。"));
            return;
        }
    }
    if (Loaded.valid()) Bindings = Loaded;
    else UE_LOG(LogTemp, Warning, TEXT("操作設定が重複しています。初期値に戻します。"));
}

bool ASimGameMode::ApplyInputBindings(const RobotSim::InputBindings& NewBindings)
{
    if (!NewBindings.valid()) return false;
    TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetNumberField(TEXT("version"), 1);
    const TCHAR* Fields[] = {
        TEXT("forward"), TEXT("backward"), TEXT("turn_left"),
        TEXT("turn_right"), TEXT("fire")
    };
    const RobotSim::InputSource Keys[] = {
        NewBindings.forward, NewBindings.backward, NewBindings.turnLeft,
        NewBindings.turnRight, NewBindings.fire
    };
    for (int32 I=0; I<5; ++I)
        Json->SetStringField(Fields[I], UTF8_TO_TCHAR(RobotSim::sourceToken(Keys[I])));
    FString Output;
    const auto Writer = TJsonWriterFactory<>::Create(&Output);
    if (!FJsonSerializer::Serialize(Json, Writer)) return false;
    const FString Path = BindingsPath();
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
    if (!FFileHelper::SaveStringToFile(
            Output, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
        return false;
    Bindings = NewBindings;
    Controller.emergencyStop();
    Actions = {};
    return true;
}

void ASimGameMode::FlushExternal()
{
    if (!Socket) return;
    uint32 Pending = 0;
    for (int32 I=0; I<4096 && Socket->HasPendingData(Pending); ++I) {
        uint8 Buffer[2048];
        int32 Read = 0;
        if (!Socket->Recv(Buffer, sizeof(Buffer), Read)) break;
    }
}

void ASimGameMode::SetSimulationPaused(bool bPause)
{
    if (bPaused == bPause) return;
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!PC) return;
    bPaused = bPause;
    bAwaitEscapeRelease = true;
    Controller.emergencyStop();
    Actions = {};
    RawInput = {};
    Accumulator = 0;
    NextInput = SimTime;
    if (bPaused) {
        if (!SettingsWidget)
            SettingsWidget = CreateWidget<USimSettingsWidget>(
                PC, USimSettingsWidget::StaticClass());
        if (!SettingsWidget) {
            bPaused = false;
            UE_LOG(LogTemp, Error, TEXT("一時停止メニューを生成できません。"));
            return;
        }
        SettingsWidget->AddToViewport(100);
        SettingsWidget->SetBindings(Bindings);
        PC->bShowMouseCursor = true;
        FInputModeGameAndUI Mode;
        Mode.SetWidgetToFocus(SettingsWidget->TakeWidget());
        Mode.SetHideCursorDuringCapture(false);
        PC->SetInputMode(Mode);
        SettingsWidget->SetKeyboardFocus();
    } else {
        SettingsWidget->RemoveFromParent();
        PC->bShowMouseCursor = false;
        PC->SetInputMode(FInputModeGameOnly());
        FlushExternal();
    }
}

void ASimGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
    if(Socket) {Socket->Close();ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);Socket=nullptr;}
    if(Log) {Log->Close();Log.Reset();}
    Super::EndPlay(Reason);
}
