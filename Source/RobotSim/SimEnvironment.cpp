#include "SimEnvironment.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace {
AActor* AddBox(UWorld* World, FVector Location, FVector Size, FLinearColor Tint,
               const TCHAR* MaterialName, bool bCollision = false)
{
    AActor* Actor = World->SpawnActor<AActor>(Location, FRotator::ZeroRotator);
    if (!Actor) return nullptr;
    auto* Mesh = NewObject<UStaticMeshComponent>(Actor);
    Actor->SetRootComponent(Mesh);
    Mesh->SetMobility(EComponentMobility::Movable);
    Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
    Mesh->SetCollisionProfileName(bCollision ? TEXT("BlockAllDynamic") : TEXT("NoCollision"));
    Mesh->SetCastShadow(false);
    Mesh->RegisterComponent();
    Actor->SetActorLocation(Location);
    Actor->SetActorScale3D(Size / 100.0);
    const FString Path = FString::Printf(TEXT("/Game/Generated/%s.%s"),
                                         MaterialName, MaterialName);
    if (auto* Base = LoadObject<UMaterialInterface>(nullptr, *Path)) {
        auto* Material = UMaterialInstanceDynamic::Create(Base, Actor);
        Material->SetVectorParameterValue(TEXT("Color"), Tint);
        Mesh->SetMaterial(0, Material);
    }
    return Actor;
}
}

void FSimEnvironment::Build(UWorld* World, AActor*& OutRobot, AActor*& OutTarget)
{
    // 外側は34m四方の屋内ホール。フィールドより低い床と構造壁を配置する。
    AddBox(World, FVector(0, 0, -18), FVector(3600, 3600, 16),
           FLinearColor(0.48f, 0.53f, 0.58f), TEXT("M_Concrete"));
    // 運動面はXY固定なので床は表示専用。これによりSweepの干渉を防止。
    AddBox(World, FVector(0, 0, -8), FVector(2000, 2000, 16),
           FLinearColor(0.63f, 0.68f, 0.68f), TEXT("M_Floor"));
    // プラダン風の板で囲んだ20mフィールド。上から屋内構造も見える高さ。
    for (int32 Sign : {-1, 1}) {
        AddBox(World, FVector(Sign * 1000, 0, 70), FVector(20, 2000, 140),
               FLinearColor(0.68f, 0.76f, 0.83f), TEXT("M_Corrugated"), true);
        AddBox(World, FVector(0, Sign * 1000, 70), FVector(2000, 20, 140),
               FLinearColor(0.63f, 0.72f, 0.80f), TEXT("M_Corrugated"), true);
    }
    // 継ぎ目、支柱、保護材を入れて人工的な単一反復模様を避ける。
    for (int32 X = -800; X <= 800; X += 400) {
        AddBox(World, FVector(X, -994, 70), FVector(12, 24, 140),
               FLinearColor(0.22f, 0.25f, 0.28f), TEXT("M_Robot"), true);
        AddBox(World, FVector(X, 994, 70), FVector(12, 24, 140),
               FLinearColor(0.22f, 0.25f, 0.28f), TEXT("M_Robot"), true);
    }
    for (int32 Y = -800; Y <= 800; Y += 400) {
        AddBox(World, FVector(-994, Y, 70), FVector(24, 12, 140),
               FLinearColor(0.22f, 0.25f, 0.28f), TEXT("M_Robot"), true);
        AddBox(World, FVector(994, Y, 70), FVector(24, 12, 140),
               FLinearColor(0.22f, 0.25f, 0.28f), TEXT("M_Robot"), true);
    }
    AddBox(World, FVector(250, 350, 60), FVector(120, 160, 120),
           FLinearColor(0.75f, 0.65f, 0.45f), TEXT("M_Corrugated"), true);
    AddBox(World, FVector(-200, -340, 42), FVector(160, 90, 84),
           FLinearColor(0.53f, 0.60f, 0.65f), TEXT("M_Concrete"), true);
    // フィールド内の床ラインとマーク。衝突しない薄い帯。
    for (int32 Y : {-570, 470}) {
        AddBox(World, FVector(0, Y, 0.4), FVector(1420, 6, 0.8),
               FLinearColor(0.95f, 0.83f, 0.34f), TEXT("M_Robot"));
    }

    // ホール外周の壁・天井と梁。フィールド境界の向こう側にも奥行きを与える。
    for (int32 Sign : {-1, 1}) {
        AddBox(World, FVector(Sign * 1800, 0, 360), FVector(25, 3600, 720),
               FLinearColor(0.71f, 0.72f, 0.69f), TEXT("M_Concrete"));
        AddBox(World, FVector(0, Sign * 1800, 360), FVector(3600, 25, 720),
               FLinearColor(0.64f, 0.69f, 0.73f), TEXT("M_Concrete"));
    }
    AddBox(World, FVector(0, 0, 730), FVector(3600, 3600, 24),
           FLinearColor(0.58f, 0.65f, 0.69f), TEXT("M_Corrugated"));
    for (int32 X : {-1500, 0, 1500}) {
        AddBox(World, FVector(X, 0, 675), FVector(24, 3550, 55),
               FLinearColor(0.27f, 0.35f, 0.40f), TEXT("M_Robot"));
    }
    for (int32 X : {-1450, 1450}) for (int32 Y : {-1450, 1450}) {
        AddBox(World, FVector(X, Y, 350), FVector(48, 48, 700),
               FLinearColor(0.33f, 0.39f, 0.43f), TEXT("M_Robot"));
    }
    // ホールの備品。フィールド外なので機体とは接触しない。
    for (int32 Index = 0; Index < 5; ++Index) {
        const float Y = -850.0f + Index * 425.0f;
        AddBox(World, FVector(1430, Y, 65), FVector(220, 150, 130),
               FLinearColor(0.61f + 0.04f * (Index % 2), 0.52f, 0.32f),
               TEXT("M_Corrugated"));
        AddBox(World, FVector(-1440, Y, 95), FVector(190, 110, 190),
               FLinearColor(0.48f, 0.57f, 0.67f), TEXT("M_Concrete"));
    }
    // 天井の白色照明パネルはUnlitでも視認できる。
    for (int32 X : {-900, 900}) for (int32 Y : {-900, 900}) {
        AddBox(World, FVector(X, Y, 711), FVector(360, 60, 4),
               FLinearColor(1.0f, 0.97f, 0.84f), TEXT("M_Robot"));
    }
    // カメラ追跡対象は赤単色を維持し、背景テクスチャの影響を切り分けやすくする。
    OutRobot = AddBox(World, FVector(-500, 0, 20), FVector(50, 40, 40),
                      FLinearColor(0.05f, 0.2f, 0.8f), TEXT("M_Robot"), true);
    OutTarget = AddBox(World, FVector(400, 0, 25), FVector(50, 50, 50),
                       FLinearColor(0.9f, 0.02f, 0.02f), TEXT("M_Robot"), true);
}
