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
    LoadInputBindings();
    CommandDelayMs = static_cast<float>(Tuning.commandDelayMs);
    VideoDelayMs = static_cast<float>(Tuning.videoDelayMs);
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
    WeaponRandom.Initialize(Seed + 104729);
    Tuning.commandDelayMs = CommandDelayMs;
    Tuning.videoDelayMs = VideoDelayMs;
    bExport=FParse::Param(FCommandLine::Get(),TEXT("ExportFrames"));
    // 独立した環境・プラント・映像モジュールを初期化する。
    AActor* SpawnedRobot = nullptr;
    AActor* SpawnedTarget = nullptr;
    AActor* SpawnedSpinningTarget = nullptr;
    FSimEnvironment::Build(GetWorld(), SpawnedRobot, SpawnedTarget,
                           SpawnedSpinningTarget);
    Robot = SpawnedRobot;
    Target = SpawnedTarget;
    SpinningTarget = SpawnedSpinningTarget;
    Vehicle.Initialize(Robot);
    AimRig.Initialize(Robot);
    Weapon.Initialize(AimRig.MuzzleMount());
    HitPanels.Initialize(Target, Tuning.panelPattern);
    SpinningHitPanels.Initialize(SpinningTarget, Tuning.panelPattern);
    Video = NewObject<URobotVideoPipeline>(this);
    Video->Initialize(Robot, AimRig.CameraMount(), bExport);
    Socket=FUdpSocketBuilder(TEXT("RobotSimCommand")).AsNonBlocking()
        .BoundToAddress(FIPv4Address(127,0,0,1)).BoundToPort(9876).WithReceiveBufferSize(65536);
    if(!Socket) UE_LOG(LogTemp,Error,TEXT("外部指令ポート9876を開けません。別のシミュレータが起動していないか確認してください。"));
    RunDirectory=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Experiments"),
        FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%S"))+TEXT("_")+FGuid::NewGuid().ToString());
    IFileManager::Get().MakeDirectory(*RunDirectory,true);
    Log.Reset(IFileManager::Get().CreateFileWriter(*FPaths::Combine(RunDirectory,TEXT("truth.csv"))));
    const FString Header=TEXT("sim_s,x_cm,y_cm,yaw_deg,speed_cm_s,yaw_deg_s,manual_throttle,manual_turn,external_throttle,external_turn,applied_throttle,applied_turn,mode,display_age_s,manual_strafe,external_strafe,applied_strafe,body_pitch,body_roll,shot_count,panel_pattern,panel_hits\n");
    if(Log) { FTCHARToUTF8 Bytes(*Header); Log->Serialize((void*)Bytes.Get(),Bytes.Length()); }
    const FString Settings = FString::Printf(
        TEXT("{\"seed\":%d,\"command_delay_ms\":%g,\"video_delay_ms\":%g,")
        TEXT("\"jitter_ms\":%g,\"loss_rate\":%g,\"shared_weight\":%g,")
        TEXT("\"export_frames\":%s,\"physics_hz\":120,\"camera_hz\":%g,")
        TEXT("\"body_sway_deg\":%g,\"shot_recoil_deg\":%g,\"tire_slip\":%g,")
        TEXT("\"turn_inertia_ms\":%g,\"panel_pattern\":%d,")
        TEXT("\"move_sensitivity\":%g,\"turn_sensitivity\":%g}"),
        Seed, CommandDelayMs, VideoDelayMs, JitterMs, LossRate, SharedWeight,
        bExport ? TEXT("true") : TEXT("false"), Tuning.displayFps,
        Tuning.bodySwayDegrees, Tuning.shotRecoilDegrees, Tuning.tireSlip,
        Tuning.turnInertiaMs, Tuning.panelPattern,
        Tuning.moveSensitivity, Tuning.turnSensitivity);
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
        double Strafe = 0;
        if (Object->HasField(TEXT("strafe")) &&
            !Object->TryGetNumberField(TEXT("strafe"), Strafe)) continue;
        Controller.submitExternal(SimTime, Seq, {T, R, Strafe});
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
            Actions = RobotSim::mapInput(Bindings, RawInput, Tuning);
            Controller.submitActions(Actions, SimTime, Delay);
        }
        RawInput.mouseX = 0;
        RawInput.mouseY = 0;
    }
    // 射撃と可視判定より先に移動・自転する2機を更新する。
    if (Target)
        Target->SetActorLocation(FVector(400, 300 * std::sin(SimTime * 0.5), 0));
    if (SpinningTarget) {
        SpinningTarget->SetActorLocation(FVector(
            160.0 + 205.0 * std::cos(0.58 * SimTime),
            -325.0 + 160.0 * std::sin(0.47 * SimTime), 0.0));
        SpinningTarget->SetActorRotation(FRotator(
            0.0, FMath::Fmod(160.0*SimTime +
                            15.0*std::sin(1.2*SimTime), 360.0), 0.0));
    }
    RobotSim::Command Applied = Controller.resolve(SimTime, SharedWeight);
    SelectedAssistPanel = -1;
    // ホールド中に限り、移動・射撃を変えず旋回・ピッチ指令のみ補助する。
    // 候補抽出は現状シミュレータのパネルコライダを使用する実証実装。
    if (Controller.assistActive() && Video && AimRig.MuzzleMount() && Robot) {
        const auto Result = FSimAimAssist::Find(
            GetWorld(), Video->CameraTransform(),
            AimRig.MuzzleMount()->GetComponentLocation(),
            Video->HorizontalFov(), Robot, Target, SpinningTarget);
        if (Result.found) {
            SelectedAssistPanel = Result.id;
            const FRotator Disturbance =
                Vehicle.BodySway() + Weapon.RecoilOffset();
            Applied.turn = RobotSim::Aim::turnCorrection(
                Robot->GetActorRotation().Yaw, Result.yawDeg - Disturbance.Yaw,
                Vehicle.YawRate(), Tuning.maxTurnRateDegPerSec());
            Applied.pitch = RobotSim::Aim::pitchCorrection(
                AimRig.PitchDegrees(), Result.pitchDeg - Disturbance.Pitch);
        }
    }
    // 緊急停止だけは通常の指令遅延・モード裁定を通さない。
    if (FSimKeyboardInput::EmergencyStop(PC)) {
        Controller.emergencyStop();
        Applied = {};
        Vehicle.ResetMotion();
        Actions.fire = false;
    }
    Weapon.Advance(Dt);
    Vehicle.Advance(Applied, Dt, Tuning);
    AimRig.AdvancePitch(Applied.pitch, Dt, Tuning.turnSensitivity);
    if (Video)
        Video->SetAimOffset(Vehicle.BodySway() + Weapon.RecoilOffset());
    if (!FSimKeyboardInput::EmergencyStop(PC))
        Weapon.TryFire(GetWorld(), Robot, SimTime, Controller.fireActive(),
                       Vehicle.BodySway(), Tuning.shotRecoilDegrees, WeaponRandom);
    if (Log && SimTime >= NextLog) {
        NextLog = SimTime + 0.02;
        const FVector P = Robot->GetActorLocation();
        const RobotSim::Command Manual = Controller.manual();
        const RobotSim::Command External = Controller.freshExternal(SimTime);
        const FString Line = FString::Printf(
            TEXT("%.6f,%.4f,%.4f,%.4f,%.4f,%.4f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%d,%.6f,%.3f,%.3f,%.3f,%.4f,%.4f,%d,%d,%d\n"),
            SimTime, P.X, P.Y, Robot->GetActorRotation().Yaw,
            Vehicle.Speed(), Vehicle.YawRate(),
            Manual.throttle, Manual.turn, External.throttle, External.turn,
            Applied.throttle, Applied.turn, static_cast<int32>(Controller.mode()),
            Video ? Video->DisplayAge(SimTime) : -1.0,
            Manual.strafe, External.strafe, Applied.strafe,
            Vehicle.BodySway().Pitch, Vehicle.BodySway().Roll, Weapon.FiredCount(),
            Tuning.panelPattern, Weapon.PanelHitCount());
        FTCHARToUTF8 Bytes(*Line);
        Log->Serialize((void*)Bytes.Get(), Bytes.Length());
    }
}
void ASimGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (auto* PC = GetWorld()->GetFirstPlayerController()) {
        if (!PC->IsInputKeyDown(EKeys::Escape)) bAwaitEscapeRelease = false;
        if (!bAwaitEscapeRelease && PC->WasInputKeyJustPressed(EKeys::Escape)) {
            SetSimulationPaused(!bPaused);
            return;
        }
        // メニュー操作中には研究モードとデバッグ表示を変更させない。
        if (!bPaused) {
            if (PC->WasInputKeyJustPressed(EKeys::One)) SwitchMode(RobotSim::Mode::Manual);
            if (PC->WasInputKeyJustPressed(EKeys::Two)) SwitchMode(RobotSim::Mode::Shared);
            if (PC->WasInputKeyJustPressed(EKeys::Three)) SwitchMode(RobotSim::Mode::Automatic);
            if (PC->WasInputKeyJustPressed(EKeys::F1)) bDebug = !bDebug;
        }
    }
    // メニュー中はシミュレーション時間、機体、映像、対象、指令経路を停止する。
    if (bPaused) return;
    const RobotSim::RawInput FrameInput =
        FSimKeyboardInput::Read(GetWorld()->GetFirstPlayerController());
    RawInput.held = FrameInput.held;
    RawInput.mouseX += FrameInput.mouseX;
    RawInput.mouseY += FrameInput.mouseY;
    // メニューをクリックして閉じた直後のクリック・マウス位置復帰を誤操作にしない。
    if (bSkipMouseFrame) {
        RawInput.mouseX = 0;
        RawInput.mouseY = 0;
        bSkipMouseFrame = false;
    }
    if (bAwaitMouseRelease) {
        const bool bLeft = FrameInput.held[RobotSim::index(RobotSim::InputSource::MouseButtonLeft)];
        const bool bRight = FrameInput.held[RobotSim::index(RobotSim::InputSource::MouseButtonRight)];
        RawInput.held[RobotSim::index(RobotSim::InputSource::MouseButtonLeft)] = false;
        RawInput.held[RobotSim::index(RobotSim::InputSource::MouseButtonRight)] = false;
        if (!bLeft && !bRight) bAwaitMouseRelease = false;
    }
    ReceiveExternal();
    // 重いフレームでは追い付ける分だけ進め、破棄時間をログに明示。
    if(DeltaSeconds>0.25f) UE_LOG(LogTemp,Warning,TEXT("SIM_OVERRUN: %.6f秒を破棄。実時間評価からこの試行を除外してください。"),DeltaSeconds-0.25f);
    Accumulator+=FMath::Min(DeltaSeconds,0.25f);
    constexpr double Dt=1.0/120.0;
    while(Accumulator>=Dt) { Step(Dt); Accumulator-=Dt; }
    if (Video) {
        Video->SetAimOffset(Vehicle.BodySway() + Weapon.RecoilOffset());
        Video->Advance(SimTime, VideoDelayMs, JitterMs, LossRate, Random, Tuning.displayFps);
    }
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
        TEXT("1:手動 2:共有 3:自動 | モード=%d | Space:非常停止 Esc:操作設定\n")
        TEXT("前後入力(手動/適用)=%+.2f/%+.2f | 速度=%+.1fcm/s | 位置=(%.0f, %.0f)cm | 移動遮断=%s\n")
        TEXT("横速度=%+.1fcm/s 射撃=%d | 指令 %.0fms 映像 %.0fms | FPS %.0f | 画像経過 %.0fms\n")
        TEXT("旋回慣性=%.0fms | パネル高さパターン=%d | パネル命中=%d\n")
        TEXT("移動感度=%.1f倍 | 旋回感度=%.1f倍"),
        static_cast<int32>(Controller.mode()),
        Controller.manual().throttle, Controller.applied().throttle,
        Vehicle.Speed(), Position.X, Position.Y,
        Vehicle.IsBlocked() ? TEXT("はい") : TEXT("いいえ"),
        Vehicle.LateralSpeed(), Weapon.FiredCount(), CommandDelayMs, VideoDelayMs,
        Tuning.displayFps,
        Video ? Video->DisplayAge(SimTime) * 1000 : -1.0,
        Tuning.turnInertiaMs, Tuning.panelPattern, Weapon.PanelHitCount(),
        Tuning.moveSensitivity, Tuning.turnSensitivity);
}

FString ASimGameMode::BindingsPath() const
{
    return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Config"),
                           TEXT("InputBindings.json"));
}

void ASimGameMode::LoadInputBindings()
{
    Bindings = {};
    Tuning = {};
    FString Json;
    if (!FFileHelper::LoadFileToString(Json, *BindingsPath())) return;
    TSharedPtr<FJsonObject> Obj;
    const auto Reader = TJsonReaderFactory<>::Create(Json);
    if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid()) {
        UE_LOG(LogTemp, Warning, TEXT("操作・物理設定のJSONが不正です。初期値を使用します。"));
        return;
    }
    RobotSim::InputBindings Loaded;
    const TCHAR* Fields[] = {
        TEXT("forward"),
        TEXT("backward"),
        TEXT("turn_left"),
        TEXT("turn_right"),
        TEXT("fire"),
        TEXT("strafe_left"),
        TEXT("strafe_right")
    };
    RobotSim::InputSource* Entries[] = {
        &Loaded.forward,
        &Loaded.backward,
        &Loaded.turnLeft,
        &Loaded.turnRight,
        &Loaded.fire,
        &Loaded.strafeLeft,
        &Loaded.strafeRight
    };
    for (int32 I=0; I<7; ++I) {
        FString Token;
        if (!Obj->TryGetStringField(Fields[I], Token)) {
            if (I >= 5) {
                // 旧版の5操作設定を読み込む。旧初期値だけ新初期値に移行する。
                if (I == 5) {
                    const bool OldDefault = Loaded.forward == RobotSim::InputSource::W &&
                        Loaded.backward == RobotSim::InputSource::S &&
                        Loaded.turnLeft == RobotSim::InputSource::A &&
                        Loaded.turnRight == RobotSim::InputSource::D &&
                        Loaded.fire == RobotSim::InputSource::MouseButtonLeft;
                    if (OldDefault) {
                        Loaded = {};
                        break;
                    }
                    Loaded.strafeLeft = RobotSim::InputSource::None;
                    Loaded.strafeRight = RobotSim::InputSource::None;
                    break;
                }
            }
            UE_LOG(LogTemp, Warning, TEXT("旧操作設定に不正な項目があるため初期値を使用します。"));
            return;
        }
        if (!RobotSim::parseSource(TCHAR_TO_UTF8(*Token), *Entries[I])) {
            UE_LOG(LogTemp, Warning, TEXT("操作割り当てが不正です。初期値を使用します。"));
            return;
        }
    }
    // v4までは7操作。追加3操作は存在すれば復元、なければ初期値。
    // 旧カスタム割当が新しい既定キーと衝突する場合は追加側を未割当にする。
    const TCHAR* ExtraFields[] = {
        TEXT("pitch_up"), TEXT("pitch_down"), TEXT("aim_assist")
    };
    RobotSim::InputSource* ExtraValues[] = {
        &Loaded.pitchUp, &Loaded.pitchDown, &Loaded.aimAssist
    };
    const RobotSim::InputSource Existing[] = {
        Loaded.forward, Loaded.backward, Loaded.strafeLeft, Loaded.strafeRight,
        Loaded.turnLeft, Loaded.turnRight, Loaded.fire
    };
    for (int32 I=0; I<3; ++I) {
        FString Token;
        if (Obj->TryGetStringField(ExtraFields[I], Token)) {
            if (!RobotSim::parseSource(TCHAR_TO_UTF8(*Token), *ExtraValues[I])) {
                UE_LOG(LogTemp, Warning, TEXT("補正・ピッチ操作設定が不正です。初期値を使用します。"));
                return;
            }
        } else {
            for (auto Key : Existing)
                if (Key == *ExtraValues[I]) *ExtraValues[I] = RobotSim::InputSource::None;
        }
    }
    if (!Loaded.valid()) {
        UE_LOG(LogTemp, Warning, TEXT("割り当て重複のため初期値を使用します。"));
        return;
    }
    Bindings = Loaded;
    RobotSim::Tuning LoadedTuning;
    const TCHAR* NumberFields[] = {
        TEXT("move_sensitivity"),
        TEXT("turn_sensitivity"),
        TEXT("mouse_button_strength"),
        TEXT("body_sway_deg"),
        TEXT("shot_recoil_deg"),
        TEXT("tire_slip"),
        TEXT("command_delay_ms"),
        TEXT("video_delay_ms"),
        TEXT("display_fps"),
        TEXT("turn_inertia_ms")
    };
    double* Values[] = {
        &LoadedTuning.moveSensitivity,
        &LoadedTuning.turnSensitivity,
        &LoadedTuning.mouseButtonStrength,
        &LoadedTuning.bodySwayDegrees,
        &LoadedTuning.shotRecoilDegrees,
        &LoadedTuning.tireSlip,
        &LoadedTuning.commandDelayMs,
        &LoadedTuning.videoDelayMs,
        &LoadedTuning.displayFps,
        &LoadedTuning.turnInertiaMs
    };
    for (int32 I=0; I<10; ++I) {
        if (Obj->HasField(NumberFields[I]) &&
            !Obj->TryGetNumberField(NumberFields[I], *Values[I])) {
            UE_LOG(LogTemp, Warning, TEXT("設定数値が不正です。数値は初期値を使用します。"));
            return;
        }
    }
    // 設定JSON v2は高さパターン未登録。v3では整数の選択値を保存する。
    double PatternValue = LoadedTuning.panelPattern;
    if (Obj->HasField(TEXT("panel_pattern"))) {
        if (!Obj->TryGetNumberField(TEXT("panel_pattern"), PatternValue) ||
            !FMath::IsFinite(PatternValue) ||
            FMath::FloorToInt(PatternValue) != PatternValue ||
            PatternValue < 0 || PatternValue >= RobotSim::PanelPatternCount) {
            UE_LOG(LogTemp, Warning, TEXT("パネル配置パターンが無効です。初期値を使用します。"));
            return;
        }
        LoadedTuning.panelPattern = static_cast<int>(PatternValue);
    }
    // v3までの「マウス1px当たり指令」を、v4の直感的な倍率へ移行する。
    // 新しいフィールドがある場合はそちらを優先し、旧フィールドを無視する。
    double Legacy = 0.0;
    if (!Obj->HasField(TEXT("move_sensitivity")) &&
        Obj->TryGetNumberField(TEXT("mouse_vertical"), Legacy) &&
        FMath::IsFinite(Legacy) && Legacy > 0) {
        LoadedTuning.moveSensitivity = FMath::Clamp(Legacy * 12.0, 0.1, 20.0);
    }
    if (!Obj->HasField(TEXT("turn_sensitivity")) &&
        Obj->TryGetNumberField(TEXT("mouse_horizontal"), Legacy) &&
        FMath::IsFinite(Legacy) && Legacy > 0) {
        LoadedTuning.turnSensitivity = FMath::Clamp(Legacy * 12.0, 0.1, 20.0);
    }
    if (LoadedTuning.valid()) Tuning = LoadedTuning;
    else UE_LOG(LogTemp, Warning, TEXT("数値範囲が不正のため初期値を使用します。"));
}

bool ASimGameMode::ApplySettings(const RobotSim::InputBindings& NewBindings,
                                  const RobotSim::Tuning& NewTuning)
{
    if (!NewBindings.valid() || !NewTuning.valid()) return false;
    TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetNumberField(TEXT("version"), 5);
    const TCHAR* Fields[] = {
        TEXT("forward"),
        TEXT("backward"),
        TEXT("turn_left"),
        TEXT("turn_right"),
        TEXT("fire"),
        TEXT("strafe_left"),
        TEXT("strafe_right")
    };
    const RobotSim::InputSource Keys[] = {
        NewBindings.forward,
        NewBindings.backward,
        NewBindings.turnLeft,
        NewBindings.turnRight,
        NewBindings.fire,
        NewBindings.strafeLeft,
        NewBindings.strafeRight
    };
    for (int32 I=0; I<7; ++I)
        Json->SetStringField(Fields[I], UTF8_TO_TCHAR(RobotSim::sourceToken(Keys[I])));
    Json->SetStringField(TEXT("pitch_up"),
        UTF8_TO_TCHAR(RobotSim::sourceToken(NewBindings.pitchUp)));
    Json->SetStringField(TEXT("pitch_down"),
        UTF8_TO_TCHAR(RobotSim::sourceToken(NewBindings.pitchDown)));
    Json->SetStringField(TEXT("aim_assist"),
        UTF8_TO_TCHAR(RobotSim::sourceToken(NewBindings.aimAssist)));
    const TCHAR* NumberFields[] = {
        TEXT("move_sensitivity"),
        TEXT("turn_sensitivity"),
        TEXT("mouse_button_strength"),
        TEXT("body_sway_deg"),
        TEXT("shot_recoil_deg"),
        TEXT("tire_slip"),
        TEXT("command_delay_ms"),
        TEXT("video_delay_ms"),
        TEXT("display_fps"),
        TEXT("turn_inertia_ms")
    };
    const double Values[] = {
        NewTuning.moveSensitivity,
        NewTuning.turnSensitivity,
        NewTuning.mouseButtonStrength,
        NewTuning.bodySwayDegrees,
        NewTuning.shotRecoilDegrees,
        NewTuning.tireSlip,
        NewTuning.commandDelayMs,
        NewTuning.videoDelayMs,
        NewTuning.displayFps,
        NewTuning.turnInertiaMs
    };
    for (int32 I=0; I<10; ++I)
        Json->SetNumberField(NumberFields[I], Values[I]);
    Json->SetNumberField(TEXT("panel_pattern"), NewTuning.panelPattern);
    FString Output;
    const auto Writer = TJsonWriterFactory<>::Create(&Output);
    if (!FJsonSerializer::Serialize(Json, Writer)) return false;
    const FString Path = BindingsPath();
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
    if (!FFileHelper::SaveStringToFile(
            Output, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
        return false;
    Bindings = NewBindings;
    Tuning = NewTuning;
    HitPanels.SetPattern(Tuning.panelPattern);
    CommandDelayMs = static_cast<float>(Tuning.commandDelayMs);
    VideoDelayMs = static_cast<float>(Tuning.videoDelayMs);
    Controller.emergencyStop();
    Actions = {};
    // 数値と単位をログの列名に明記。感度も新しい2軸のみを記録する。
    const FString HistoryPath =
        FPaths::Combine(RunDirectory, TEXT("parameter_changes.csv"));
    if (!IFileManager::Get().FileExists(*HistoryPath)) {
        FFileHelper::SaveStringToFile(
            TEXT("sim_s,body_sway_deg,shot_recoil_deg,tire_slip,mouse_button_strength,")
            TEXT("command_delay_ms,video_delay_ms,display_fps,turn_inertia_ms,")
            TEXT("panel_pattern,move_sensitivity,turn_sensitivity\n"),
            *HistoryPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }
    const FString History = FString::Printf(
        TEXT("%.6f,%.3f,%.3f,%.2f,%.3f,%.0f,%.0f,%.1f,%.1f,%d,%.1f,%.1f\n"),
        SimTime, Tuning.bodySwayDegrees, Tuning.shotRecoilDegrees,
        Tuning.tireSlip, Tuning.mouseButtonStrength,
        Tuning.commandDelayMs, Tuning.videoDelayMs, Tuning.displayFps,
        Tuning.turnInertiaMs, Tuning.panelPattern,
        Tuning.moveSensitivity, Tuning.turnSensitivity);
    FFileHelper::SaveStringToFile(History,
        *HistoryPath,
        FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(),
        FILEWRITE_Append);
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
        SettingsWidget->SetSettings(Bindings, Tuning);
        PC->bShowMouseCursor = true;
        FInputModeGameAndUI Mode;
        Mode.SetWidgetToFocus(SettingsWidget->TakeWidget());
        Mode.SetHideCursorDuringCapture(false);
        PC->SetInputMode(Mode);
        SettingsWidget->SetKeyboardFocus();
    } else {
        SettingsWidget->RemoveFromParent();
        bAwaitMouseRelease = true;
        bSkipMouseFrame = true;
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
