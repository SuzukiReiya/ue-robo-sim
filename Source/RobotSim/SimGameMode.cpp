#include "SimGameMode.h"
#include "SimEnvironment.h"
#include "SimKeyboardInput.h"
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
    // 操作状態の取得は入力アダプタ、指令への変換と遅延・裁定はController。
    if (SimTime >= NextInput) {
        NextInput = SimTime + 0.02;
        if (Random.FRand() >= LossRate) {
            const double Delay = FMath::Max(
                0.0f, CommandDelayMs + Random.FRandRange(-JitterMs, JitterMs)) / 1000.0;
            Controller.submitButtons(FSimKeyboardInput::Read(PC), SimTime, Delay);
        }
    }
    RobotSim::Command Applied = Controller.resolve(SimTime, SharedWeight);
    // 緊急停止だけは通常の指令遅延・モード裁定を通さない。
    if (FSimKeyboardInput::EmergencyStop(PC)) {
        Controller.emergencyStop();
        Applied = {};
        Vehicle.ResetMotion();
    }
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
        if(PC->WasInputKeyJustPressed(EKeys::One)) SwitchMode(RobotSim::Mode::Manual);
        if(PC->WasInputKeyJustPressed(EKeys::Two)) SwitchMode(RobotSim::Mode::Shared);
        if(PC->WasInputKeyJustPressed(EKeys::Three)) SwitchMode(RobotSim::Mode::Automatic);
        if(PC->WasInputKeyJustPressed(EKeys::F1)) bDebug=!bDebug;
        if(PC->WasInputKeyJustPressed(EKeys::Escape)) PC->ConsoleCommand(TEXT("quit"));
    }
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
        TEXT("指令 %.0fms 映像 %.0fms | 画像経過 %.0fms | F1:表示切替"),
        static_cast<int32>(Controller.mode()),
        Controller.manual().throttle, Controller.applied().throttle,
        Vehicle.Speed(), Position.X, Position.Y,
        Vehicle.IsBlocked() ? TEXT("はい") : TEXT("いいえ"),
        CommandDelayMs, VideoDelayMs,
        Video ? Video->DisplayAge(SimTime) * 1000 : -1.0);
}
void ASimGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
    if(Socket) {Socket->Close();ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);Socket=nullptr;}
    if(Log) {Log->Close();Log.Reset();}
    Super::EndPlay(Reason);
}
void ASimHUD::DrawHUD()
{
    Super::DrawHUD();
    auto* Sim=Cast<ASimGameMode>(GetWorld()->GetAuthGameMode());
    if(!Canvas||!Sim) return;
    DrawRect(FLinearColor::Black,0,0,Canvas->SizeX,Canvas->SizeY);
    const float W=FMath::Min(static_cast<float>(Canvas->SizeX),Canvas->SizeY*16.0f/9.0f);
    const float H=W*9.0f/16.0f;
    if(auto* Texture=Sim->DisplayTexture()) DrawTexture(Texture,(Canvas->SizeX-W)/2,(Canvas->SizeY-H)/2,W,H,0,0,1,1,FLinearColor::White,BLEND_Opaque);
    const FString Info=Sim->Status();
    if(!Info.IsEmpty()) {
        TArray<FString> Lines;
        Info.ParseIntoArrayLines(Lines,false);
        for(int32 I=0;I<Lines.Num();++I)
            DrawText(Lines[I],FLinearColor::Yellow,16,16+24*I,nullptr,1.0f);
    }
}
