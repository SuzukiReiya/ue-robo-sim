#include "SimEnvironment.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/PointLight.h"
#include "Components/PointLightComponent.h"

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
    // 床・外側設備は描画のみ。衝突物の立体的な陰影は許可する。
    Mesh->SetCastShadow(bCollision);
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

// ゲームモジュールを規定の高さ・方向に固定できる、無拡大スケールの敵シャーシ。
AActor* AddTargetChassis(UWorld* World, FVector SpawnLocation, FLinearColor Tint)
{
    auto* Actor = World->SpawnActor<AActor>(
        SpawnLocation, FRotator::ZeroRotator);
    if (!Actor) return nullptr;
    auto* Root = NewObject<USceneComponent>(Actor);
    Actor->SetRootComponent(Root);
    Root->RegisterComponent();
    auto* Body = NewObject<UStaticMeshComponent>(Actor);
    Body->SetupAttachment(Root);
    Body->SetStaticMesh(LoadObject<UStaticMesh>(
        nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
    Body->SetRelativeLocation(FVector(0, 0, 5.0f));
    Body->SetRelativeScale3D(FVector(0.18f, 0.18f, 0.08f));
    Body->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Body->RegisterComponent();
    if (auto* Base = LoadObject<UMaterialInterface>(
            nullptr, TEXT("/Game/Generated/M_Robot.M_Robot"))) {
        auto* Color = UMaterialInstanceDynamic::Create(Base, Actor);
        Color->SetVectorParameterValue(
            TEXT("Color"), Tint);
        Body->SetMaterial(0, Color);
    }
    return Actor;
}
// 操作対象も無スケールRootで構築。これによりカメラ・射出口のcm座標が正確になる。
AActor* AddPlayerChassis(UWorld* World)
{
    auto* Actor = AddTargetChassis(
        World, FVector(-500.0f, 0.0f, 0.0f),
        FLinearColor(0.04f, 0.19f, 0.65f));
    if (Actor) {
        if (auto* Mesh = Actor->FindComponentByClass<UStaticMeshComponent>()) {
            Mesh->SetRelativeScale3D(FVector(0.20f,0.18f,0.10f));
        }
    }
    return Actor;
}
}

void FSimEnvironment::Build(UWorld* World, AActor*& OutRobot,
                            AActor*& OutTarget, AActor*& OutSpinningTarget)
{
    // PBR材質用の屋内基準照明。Lumen無効のため、主光＋影を落とさない補助光。
    auto AddLight = [World](FRotator Rotation, float Intensity, bool bShadows) {
        if (auto* Light = World->SpawnActor<ADirectionalLight>(
                FVector(0, 0, 800), Rotation)) {
            auto* Component = Light->GetLightComponent();
            Component->SetMobility(EComponentMobility::Movable);
            Component->SetIntensity(Intensity);
            Component->SetCastShadows(bShadows);
        }
    };
    AddLight(FRotator(-70, -30, 0), 5.0f, true);
    AddLight(FRotator(-35, 135, 0), 1.6f, false);
    AddLight(FRotator(35, -15, 0), 1.0f, false);
    // 暗い場内の壁面へ間接光のような色味を付ける小型の補助灯。
    auto AddAccentLamp = [World](FVector Position, FLinearColor Tint) {
        auto* Lamp = World->SpawnActor<APointLight>(Position, FRotator::ZeroRotator);
        if (!Lamp) return;
        auto* Component = Lamp->GetPointLightComponent();
        Component->SetMobility(EComponentMobility::Movable);
        Component->SetLightColor(Tint);
        Component->SetIntensity(1100.0f);
        Component->SetAttenuationRadius(550.0f);
        Component->SetCastShadows(false);
    };
    for (int32 Side : {-1, 1}) {
        AddAccentLamp(FVector(Side * 965.0f, -600.0f, 115.0f),
                      FLinearColor(0.10f,0.45f,1.0f));
        AddAccentLamp(FVector(Side * 965.0f, 600.0f, 115.0f),
                      FLinearColor(0.85f,0.18f,0.44f));
        AddAccentLamp(FVector(-600.0f, Side * 965.0f, 115.0f),
                      FLinearColor(0.08f,0.55f,0.68f));
        AddAccentLamp(FVector(600.0f, Side * 965.0f, 115.0f),
                      FLinearColor(0.70f,0.30f,0.85f));
    }
    // 外側は34m四方の屋内ホール。フィールドより低い床と構造壁を配置する。
    AddBox(World, FVector(0, 0, -18), FVector(3600, 3600, 16),
           FLinearColor(0.48f, 0.53f, 0.58f), TEXT("M_Concrete_PBRv2"));
    // 運動面はXY固定なので床は表示専用。これによりSweepの干渉を防止。
    AddBox(World, FVector(0, 0, -8), FVector(2000, 2000, 16),
           FLinearColor(0.63f, 0.68f, 0.68f), TEXT("M_Floor_PBRv2"));
    // プラダン風の板で囲んだ20mフィールド。上から屋内構造も見える高さ。
    for (int32 Sign : {-1, 1}) {
        AddBox(World, FVector(Sign * 1000, 0, 70), FVector(20, 2000, 140),
               FLinearColor(0.68f, 0.76f, 0.83f), TEXT("M_Corrugated_PBRv2"), true);
        AddBox(World, FVector(0, Sign * 1000, 70), FVector(2000, 20, 140),
               FLinearColor(0.63f, 0.72f, 0.80f), TEXT("M_Corrugated_PBRv2"), true);
    }
    // 壁面沿いの帯状発光。壁の表面そのものを過度に白飛びさせず発光色を示す。
    for (int32 Side : {-1, 1}) {
        AddBox(World, FVector(Side * 988, 0, 112), FVector(4, 1820, 4),
               FLinearColor(0.08f, 0.65f, 1.55f), TEXT("M_Robot"));
        AddBox(World, FVector(0, Side * 988, 112), FVector(1820, 4, 4),
               FLinearColor(1.2f, 0.24f, 0.52f), TEXT("M_Robot"));
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
           FLinearColor(0.75f, 0.65f, 0.45f), TEXT("M_Corrugated_PBRv2"), true);
    AddBox(World, FVector(-200, -340, 42), FVector(160, 90, 84),
           FLinearColor(0.53f, 0.60f, 0.65f), TEXT("M_Concrete_PBRv2"), true);
    // フィールド内の床ラインとマーク。衝突しない薄い帯。
    for (int32 Y : {-570, 470}) {
        AddBox(World, FVector(0, Y, 0.4), FVector(1420, 6, 0.8),
               FLinearColor(0.95f, 0.83f, 0.34f), TEXT("M_Robot"));
    }

    // ホール外周の壁・天井と梁。フィールド境界の向こう側にも奥行きを与える。
    for (int32 Sign : {-1, 1}) {
        AddBox(World, FVector(Sign * 1800, 0, 360), FVector(25, 3600, 720),
               FLinearColor(0.71f, 0.72f, 0.69f), TEXT("M_Concrete_PBRv2"));
        AddBox(World, FVector(0, Sign * 1800, 360), FVector(3600, 25, 720),
               FLinearColor(0.64f, 0.69f, 0.73f), TEXT("M_Concrete_PBRv2"));
    }
    AddBox(World, FVector(0, 0, 730), FVector(3600, 3600, 24),
           FLinearColor(0.22f, 0.26f, 0.31f), TEXT("M_Robot"));
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
               TEXT("M_Corrugated_PBRv2"));
        AddBox(World, FVector(-1440, Y, 95), FVector(190, 110, 190),
               FLinearColor(0.48f, 0.57f, 0.67f), TEXT("M_Concrete_PBRv2"));
    }
    // 天井照明パネルと追跡対象は、検証のため単色Unlitを維持。
    for (int32 X : {-900, 900}) for (int32 Y : {-900, 900}) {
        AddBox(World, FVector(X, Y, 711), FVector(360, 60, 4),
               FLinearColor(1.0f, 0.97f, 0.84f), TEXT("M_Robot"));
    }
    // 自機と敵2機のRootは1.0倍。パネル・砲口の相対位置に縮尺誤差を入れない。
    OutRobot = AddPlayerChassis(World);
    OutTarget = AddTargetChassis(
        World, FVector(400, 0, 0), FLinearColor(0.72f,0.05f,0.06f));
    OutSpinningTarget = AddTargetChassis(
        World, FVector(160, -330, 0), FLinearColor(0.62f,0.14f,0.06f));
}
