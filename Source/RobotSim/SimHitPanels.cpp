#include "SimHitPanels.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Math/RotationMatrix.h"

namespace {
UStaticMesh* CubeMesh()
{
    return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
}
void SetTint(UStaticMeshComponent* Component, AActor* Owner, FLinearColor Color)
{
    if (auto* Base = LoadObject<UMaterialInterface>(
            nullptr, TEXT("/Game/Generated/M_Robot.M_Robot"))) {
        auto* Dynamic = UMaterialInstanceDynamic::Create(Base, Owner);
        Dynamic->SetVectorParameterValue(TEXT("Color"), Color);
        Component->SetMaterial(0, Dynamic);
    }
}
}

// STEPメッシュ: ローカルXYが表面、+Zが前面。75°は地表面に対するパネル面の角度。
void FSimHitPanels::Initialize(AActor* Target, int Pattern)
{
    Owner = Target;
    Anchors.Empty();
    if (!Target || !Target->GetRootComponent()) return;
    const auto* PanelMesh = LoadObject<UStaticMesh>(
        nullptr, TEXT("/Game/Generated/SM_GroundHitPanelModule.SM_GroundHitPanelModule"));
    if (!PanelMesh) {
        UE_LOG(LogTemp, Warning,
               TEXT("ヒットパネルOBJが未生成のため90mm角の簡易パネルを使用します。"));
    }
    const FVector Direction[] = {
        FVector(1,0,0), FVector(0,1,0), FVector(-1,0,0), FVector(0,-1,0)
    };
    constexpr float Deg = 15.0f; // 水平から75° = 鉛直から15°
    const float C = FMath::Cos(FMath::DegreesToRadians(Deg));
    const float S = FMath::Sin(FMath::DegreesToRadians(Deg));
    for (int32 Side=0; Side<4; ++Side) {
        const FName Name(*FString::Printf(TEXT("HitPanelPivot_%d"), Side));
        auto* Pivot = NewObject<USceneComponent>(Target, Name);
        Pivot->SetupAttachment(Target->GetRootComponent());
        Pivot->RegisterComponent();
        const FVector Out = Direction[Side];
        const FVector Right = FVector::CrossProduct(FVector::UpVector, Out);
        const FVector FaceNormal = Out*C + FVector::UpVector*S;
        Pivot->SetRelativeRotation(FRotationMatrix::MakeFromXZ(Right, FaceNormal).Rotator());
        Anchors.Add(Pivot);
        auto* Shape = NewObject<UStaticMeshComponent>(
            Target, *FString::Printf(TEXT("HitPanelGeometry_%d"), Side));
        Shape->SetupAttachment(Pivot);
        Shape->SetStaticMesh(PanelMesh ? const_cast<UStaticMesh*>(PanelMesh) : CubeMesh());
        if (!PanelMesh)
            Shape->SetRelativeScale3D(FVector(0.09f,0.09f,0.019f));
        Shape->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Shape->SetCastShadow(true);
        Shape->RegisterComponent();
        SetTint(Shape, Target, FLinearColor(0.07f,0.12f,0.14f));

        // STEP由来フレームの上へ識別面を描画。発光のみで暗い屋内でも視認可能。
        auto* Face = NewObject<UStaticMeshComponent>(
            Target, *FString::Printf(TEXT("HitPanelFace_%d"), Side));
        Face->SetupAttachment(Pivot);
        Face->SetStaticMesh(CubeMesh());
        Face->SetRelativeLocation(FVector(0,0,1.3f));
        Face->SetRelativeScale3D(FVector(0.071f,0.071f,0.001f));
        Face->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Face->SetCastShadow(false);
        Face->RegisterComponent();
        SetTint(Face, Target, FLinearColor(0.04f,0.83f,0.78f));

        // 射撃のVisibilityレイキャストのみ受け付ける判定面。
        // 物理車体やその他の衝突チャンネルには影響しない。
        auto* Sensor = NewObject<UBoxComponent>(
            Target, *FString::Printf(TEXT("HitPanelCollider_%d"), Side));
        Sensor->SetupAttachment(Pivot);
        Sensor->SetRelativeLocation(FVector(0,0,1.37f));
        Sensor->SetBoxExtent(FVector(4.45f,4.45f,0.08f));
        Sensor->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Sensor->SetCollisionResponseToAllChannels(ECR_Ignore);
        Sensor->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        Sensor->ComponentTags.Add(FName(TEXT("HitPanel")));
        Sensor->SetHiddenInGame(true);
        Sensor->RegisterComponent();
    }
    SetPattern(Pattern);
}
void FSimHitPanels::SetPattern(int Pattern)
{
    if (!RobotSim::panelPatternIsValid(Pattern)) return;
    CurrentPattern = Pattern;
    const FVector Directions[] = {
        FVector(1,0,0), FVector(0,1,0), FVector(-1,0,0), FVector(0,-1,0)
    };
    for (int32 Side=0; Side<Anchors.Num() && Side<4; ++Side) {
        if (auto* Pivot = Anchors[Side].Get()) {
            const float Cm = static_cast<float>(RobotSim::panelHeightCm(Pattern, Side));
            Pivot->SetRelativeLocation(
                Directions[Side]*RobotSim::PanelRadialOffsetCm + FVector(0,0,Cm));
        }
    }
    UE_LOG(LogTemp, Display, TEXT("SIM_PANEL_LAYOUT: パターン=%d 高さ=%.1f/%.1f/%.1f/%.1fcm"),
           Pattern,
           RobotSim::panelHeightCm(Pattern,0), RobotSim::panelHeightCm(Pattern,1),
           RobotSim::panelHeightCm(Pattern,2), RobotSim::panelHeightCm(Pattern,3));
}
