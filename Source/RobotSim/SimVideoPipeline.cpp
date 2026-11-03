#include "SimVideoPipeline.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GameFramework/Actor.h"
#include "ImageUtils.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

void URobotVideoPipeline::Initialize(AActor* CameraRobot, bool bEnableExport)
{
    bExport = bEnableExport;
    Sensor = NewObject<USceneCaptureComponent2D>(CameraRobot);
    Sensor->SetupAttachment(CameraRobot->GetRootComponent());
    Sensor->SetAbsolute(false, false, true);
    Sensor->SetRelativeLocation(FVector(60, 0, 80));
    Sensor->bCaptureEveryFrame = false;
    Sensor->bCaptureOnMovement = false;
    Sensor->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
    Sensor->FOVAngle = 90;
    Sensor->PostProcessSettings.bOverride_AutoExposureMethod = true;
    Sensor->PostProcessSettings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
    Sensor->PostProcessSettings.bOverride_AutoExposureBias = true;
    Sensor->PostProcessSettings.AutoExposureBias = 0;
    Sensor->RegisterComponent();
    Sensor->HideActorComponents(CameraRobot);
    // カメラは640×360、最大20fps。表示待ちのフレームは上書きしない。
    for (int32 I = 0; I < 32; ++I) {
        auto* Texture = NewObject<UTextureRenderTarget2D>(this);
        Texture->RenderTargetFormat = RTF_RGBA8;
        Texture->InitAutoFormat(640, 360);
        Texture->UpdateResourceImmediate(true);
        Frames.Add(Texture);
    }
}

void URobotVideoPipeline::Capture(double SimTime, float VideoDelayMs,
                                  float JitterMs, float LossRate, FRandomStream& Random)
{
    int32 Slot = INDEX_NONE;
    for (int32 Count = 0; Count < Frames.Num(); ++Count) {
        const int32 Candidate = (NextIndex + Count) % Frames.Num();
        if (Candidate == DisplayIndex) continue;
        if (Pending.ContainsByPredicate([Candidate](const FPendingFrame& F) {
                return F.Index == Candidate;
            })) continue;
        Slot = Candidate;
        break;
    }
    if (Slot == INDEX_NONE) return;
    NextIndex = (Slot + 1) % Frames.Num();
    Sensor->TextureTarget = Frames[Slot];
    Sensor->CaptureScene();
    ++FrameId;
    if (bExport) ExportFrame(Frames[Slot], FrameId, SimTime);
    if (Random.FRand() >= LossRate)
        Pending.Add({Slot,
                     SimTime + FMath::Max(0.0f,
                         VideoDelayMs + Random.FRandRange(-JitterMs, JitterMs)) / 1000.0,
                     SimTime});
}

void URobotVideoPipeline::Advance(double SimTime, float VideoDelayMs,
                                  float JitterMs, float LossRate, FRandomStream& Random,
                                  double DisplayFps)
{
    if (SimTime >= NextCapture) {
        Capture(SimTime, VideoDelayMs, JitterMs, LossRate, Random);
        NextCapture = SimTime + 1.0 / FMath::Clamp(DisplayFps, 1.0, 60.0);
    }
    for (int32 I = Pending.Num() - 1; I >= 0; --I) {
        const FPendingFrame F = Pending[I];
        if (F.Due <= SimTime) {
            if (DisplayIndex < 0 || F.Shot >= DisplayShot) {
                DisplayIndex = F.Index;
                DisplayShot = F.Shot;
            }
            Pending.RemoveAt(I);
        }
    }
}

UTextureRenderTarget2D* URobotVideoPipeline::DisplayTexture() const
{
    return Frames.IsValidIndex(DisplayIndex) ? Frames[DisplayIndex].Get() : nullptr;
}

double URobotVideoPipeline::DisplayAge(double SimTime) const
{
    return DisplayIndex < 0 ? -1.0 : SimTime - DisplayShot;
}

void URobotVideoPipeline::ExportFrame(UTextureRenderTarget2D* Texture,
                                      uint64 Id, double ShotTime)
{
    TArray<FColor> Pixels;
    if (!Texture->GameThread_GetRenderTargetResource()->ReadPixels(Pixels)) return;
    TArray<uint8> Png;
    FImageUtils::CompressImageArray(Texture->SizeX, Texture->SizeY, Pixels, Png);
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Sensor"));
    IFileManager::Get().MakeDirectory(*Directory, true);
    const FString Name = FString::Printf(TEXT("frame_%d.png"), static_cast<int32>(Id % 2));
    const FString Image = FPaths::Combine(Directory, Name);
    if (!FFileHelper::SaveArrayToFile(Png, *(Image + TEXT(".tmp")))) return;
    if (!IFileManager::Get().Move(*Image, *(Image + TEXT(".tmp")), true)) return;
    const FString Manifest = FString::Printf(
        TEXT("{\"id\":%llu,\"capture_sim_s\":%.9f,\"image\":\"%s\",\"width\":640,\"height\":360}"),
        static_cast<unsigned long long>(Id), ShotTime, *Name);
    const FString Path = FPaths::Combine(Directory, TEXT("latest.json"));
    if (FFileHelper::SaveStringToFile(Manifest, *(Path + TEXT(".tmp")),
                                      FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
        IFileManager::Get().Move(*Path, *(Path + TEXT(".tmp")), true);
}

void URobotVideoPipeline::SetAimOffset(const FRotator& Offset)
{
    if (Sensor) Sensor->SetRelativeRotation(Offset);
}
