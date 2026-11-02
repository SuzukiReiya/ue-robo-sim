#include "SimGameMode.h"
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
AActor* ASimGameMode::MakeBox(FVector Location,FVector Size,FLinearColor Color)
{
    AActor* A=GetWorld()->SpawnActor<AActor>(Location,FRotator::ZeroRotator);
    auto* Mesh=NewObject<UStaticMeshComponent>(A);
    A->SetRootComponent(Mesh);
    Mesh->SetMobility(EComponentMobility::Movable);
    Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
    Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Mesh->RegisterComponent();
    A->SetActorLocation(Location);
    A->SetActorScale3D(Size/100.0);
    // マップ生成時に作成した色パラメータ付き材質。
    if(auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Generated/M_Robot.M_Robot"))) {
        auto* Material=UMaterialInstanceDynamic::Create(Base,A);
        Material->SetVectorParameterValue(TEXT("Color"),Color);
        Mesh->SetMaterial(0,Material);
    }
    return A;
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
    MakeBox(FVector(0,0,-10),FVector(2000,2000,20),FLinearColor(0.3f,0.3f,0.3f));
    for(int32 Sign : {-1,1}) {
        MakeBox(FVector(Sign*1000,0,100),FVector(20,2000,200),FLinearColor(0.6f,0.6f,0.6f));
        MakeBox(FVector(0,Sign*1000,100),FVector(2000,20,200),FLinearColor(0.6f,0.6f,0.6f));
    }
    MakeBox(FVector(250,350,60),FVector(120,160,120),FLinearColor(0.7f,0.6f,0.2f));
    Robot=MakeBox(FVector(-500,0,20),FVector(50,40,40),FLinearColor(0.05f,0.2f,0.8f));
    Target=MakeBox(FVector(400,0,25),FVector(50,50,50),FLinearColor(0.9f,0.02f,0.02f));
    auto* Light=GetWorld()->SpawnActor<ADirectionalLight>(FVector(0,0,800),FRotator(-55,-25,0));
    Light->GetLightComponent()->SetMobility(EComponentMobility::Movable);
    Light->GetLightComponent()->SetIntensity(5);
    Sensor=NewObject<USceneCaptureComponent2D>(Robot);
    Sensor->SetupAttachment(Robot->GetRootComponent());
    Sensor->SetAbsolute(false,false,true);
    Sensor->SetRelativeLocation(FVector(60,0,80));
    Sensor->bCaptureEveryFrame=false;
    Sensor->bCaptureOnMovement=false;
    Sensor->CaptureSource=ESceneCaptureSource::SCS_FinalColorLDR;
    Sensor->FOVAngle=90;
    Sensor->PostProcessSettings.bOverride_AutoExposureMethod=true;
    Sensor->PostProcessSettings.AutoExposureMethod=EAutoExposureMethod::AEM_Manual;
    Sensor->PostProcessSettings.bOverride_AutoExposureBias=true;
    Sensor->PostProcessSettings.AutoExposureBias=8;
    Sensor->RegisterComponent();
    Sensor->HideActorComponents(Robot);
    // 最大遅延1.2秒・20fpsに対し十分な余裕。待機画像を上書きしない。
    for(int32 I=0;I<32;++I) {
        auto* Texture=NewObject<UTextureRenderTarget2D>(this);
        Texture->RenderTargetFormat=RTF_RGBA8;
        Texture->InitAutoFormat(640,360);
        Texture->UpdateResourceImmediate(true);
        Frames.Add(Texture);
    }
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
void ASimGameMode::SwitchMode(int32 NewMode)
{
    Mode=NewMode;
    // 古い入力や切替前の外部指令を引き継がない。
    Commands.clear();Manual={};External={};Applied={};LastExternal=-100;
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
        if(!FMath::IsFinite(Seq)||!FMath::IsFinite(T)||!FMath::IsFinite(R)||Seq<=LastSequence) continue;
        LastSequence=Seq; LastExternal=SimTime;
        External={FMath::Clamp(T,-1.0,1.0),FMath::Clamp(R,-1.0,1.0)};
    }
}
void ASimGameMode::Step(double Dt)
{
    SimTime+=Dt;
    auto* PC=GetWorld()->GetFirstPlayerController();
    if(SimTime>=NextInput) {
        NextInput=SimTime+0.02;
        RobotSim::Command Input;
        if(PC) {
            Input.throttle=(PC->IsInputKeyDown(EKeys::W)?1.0:0.0)-(PC->IsInputKeyDown(EKeys::S)?1.0:0.0);
            Input.turn=(PC->IsInputKeyDown(EKeys::D)?1.0:0.0)-(PC->IsInputKeyDown(EKeys::A)?1.0:0.0);
        }
        if(Random.FRand()>=LossRate) Commands.push(SimTime,FMath::Max(0.0f,CommandDelayMs+Random.FRandRange(-JitterMs,JitterMs))/1000.0,Input);
    }
    if(Commands.poll(SimTime,Manual)) LastManual=SimTime;
    if(SimTime-LastManual>0.25) Manual={};
    const bool Fresh=SimTime-LastExternal<=0.25;
    const RobotSim::Command Local=Fresh?External:RobotSim::Command{};
    Applied=Mode==0?Manual:(Mode==1?(Fresh?RobotSim::blend(Manual,Local,SharedWeight):Manual):Local);
    // Spaceは検証用の遅延なし非常停止。通常運用の通信経路とは区別する。
    if(PC && PC->IsInputKeyDown(EKeys::SpaceBar)) {
        Applied={};Drive={};Commands.clear();Manual={};
    }
    Drive.step(Applied,Dt,150,120,0.15);
    const double Angle=FMath::DegreesToRadians(Robot->GetActorRotation().Yaw+Drive.yawRate*Dt*0.5);
    FHitResult Hit;
    Robot->AddActorWorldOffset(FVector(std::cos(Angle),std::sin(Angle),0)*Drive.speed*Dt,true,&Hit);
    if(Hit.bBlockingHit) Drive.speed=0;
    // 初期版では旋回の形状Sweepは行わない。精密接触の評価対象外。
    Robot->AddActorWorldRotation(FRotator(0,Drive.yawRate*Dt,0));
    Target->SetActorLocation(FVector(400,300*std::sin(SimTime*0.5),25));
    if(Log && SimTime>=NextLog) {
        NextLog=SimTime+0.02;
        const FVector P=Robot->GetActorLocation();
        const FString Line=FString::Printf(TEXT("%.6f,%.4f,%.4f,%.4f,%.4f,%.4f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%d,%.6f\n"),SimTime,P.X,P.Y,Robot->GetActorRotation().Yaw,Drive.speed,Drive.yawRate,Manual.throttle,Manual.turn,Local.throttle,Local.turn,Applied.throttle,Applied.turn,Mode,DisplayIndex<0?-1.0:SimTime-DisplayShot);
        FTCHARToUTF8 Bytes(*Line); Log->Serialize((void*)Bytes.Get(),Bytes.Length());
    }
}
void ASimGameMode::CaptureFrame()
{
    int32 Slot=INDEX_NONE;
    for(int32 Count=0;Count<Frames.Num();++Count) {
        const int32 Candidate=(NextIndex+Count)%Frames.Num();
        if(Candidate==DisplayIndex) continue;
        if(Pending.ContainsByPredicate([Candidate](const PendingFrame& F){return F.Index==Candidate;})) continue;
        Slot=Candidate; break;
    }
    if(Slot==INDEX_NONE) return;
    NextIndex=(Slot+1)%Frames.Num();
    Sensor->TextureTarget=Frames[Slot];
    Sensor->CaptureScene();
    ++FrameId;
    if(bExport) ExportFrame(Frames[Slot],FrameId,SimTime);
    if(Random.FRand()>=LossRate) Pending.Add({Slot,SimTime+FMath::Max(0.0f,VideoDelayMs+Random.FRandRange(-JitterMs,JitterMs))/1000.0,SimTime,FrameId});
}
void ASimGameMode::ExportFrame(UTextureRenderTarget2D* Texture,uint64 Id,double ShotTime)
{
    TArray<FColor> Pixels;
    if(!Texture->GameThread_GetRenderTargetResource()->ReadPixels(Pixels)) return;
    TArray<uint8> Png;
    FImageUtils::CompressImageArray(Texture->SizeX,Texture->SizeY,Pixels,Png);
    const FString Directory=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Sensor"));
    IFileManager::Get().MakeDirectory(*Directory,true);
    // 2世代の画像と最後に公開するマニフェスト。読取側も世代番号を再確認する。
    const FString Name=FString::Printf(TEXT("frame_%d.png"),static_cast<int32>(Id%2));
    const FString Image=FPaths::Combine(Directory,Name);
    if(!FFileHelper::SaveArrayToFile(Png,*(Image+TEXT(".tmp")))) return;
    if(!IFileManager::Get().Move(*Image,*(Image+TEXT(".tmp")),true)) return;
    const FString Manifest=FString::Printf(TEXT("{\"id\":%llu,\"capture_sim_s\":%.9f,\"image\":\"%s\",\"width\":640,\"height\":360}"),static_cast<unsigned long long>(Id),ShotTime,*Name);
    const FString Path=FPaths::Combine(Directory,TEXT("latest.json"));
    if(FFileHelper::SaveStringToFile(Manifest,*(Path+TEXT(".tmp")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
        IFileManager::Get().Move(*Path,*(Path+TEXT(".tmp")),true);
}
void ASimGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if(auto* PC=GetWorld()->GetFirstPlayerController()) {
        if(PC->WasInputKeyJustPressed(EKeys::One)) SwitchMode(0);
        if(PC->WasInputKeyJustPressed(EKeys::Two)) SwitchMode(1);
        if(PC->WasInputKeyJustPressed(EKeys::Three)) SwitchMode(2);
        if(PC->WasInputKeyJustPressed(EKeys::F1)) bDebug=!bDebug;
        if(PC->WasInputKeyJustPressed(EKeys::Escape)) PC->ConsoleCommand(TEXT("quit"));
    }
    ReceiveExternal();
    // 重いフレームでは追い付ける分だけ進め、破棄時間をログに明示。
    if(DeltaSeconds>0.25f) UE_LOG(LogTemp,Warning,TEXT("SIM_OVERRUN: %.6f秒を破棄。実時間評価からこの試行を除外してください。"),DeltaSeconds-0.25f);
    Accumulator+=FMath::Min(DeltaSeconds,0.25f);
    constexpr double Dt=1.0/120.0;
    while(Accumulator>=Dt) { Step(Dt); Accumulator-=Dt; }
    if(SimTime>=NextCapture) {CaptureFrame();NextCapture=SimTime+0.05;}
    for(int32 I=Pending.Num()-1;I>=0;--I) {
        const auto Frame=Pending[I];
        if(Frame.Due<=SimTime) {
            if(DisplayIndex<0||Frame.Shot>=DisplayShot) { DisplayIndex=Frame.Index;DisplayShot=Frame.Shot; }
            Pending.RemoveAt(I);
        }
    }
}
UTextureRenderTarget2D* ASimGameMode::DisplayTexture() const
{
    return Frames.IsValidIndex(DisplayIndex)?Frames[DisplayIndex].Get():nullptr;
}
FString ASimGameMode::Status() const
{
    if(!bDebug) return FString();
    return FString::Printf(TEXT("1:手動 2:共有 3:自動 | モード=%d | 指令 %.0fms 映像 %.0fms | 画像経過 %.0fms | F1:表示切替"),Mode,CommandDelayMs,VideoDelayMs,(SimTime-DisplayShot)*1000);
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
    if(!Info.IsEmpty()) DrawText(Info,FLinearColor::Yellow,16,16,nullptr,1.0f);
}
