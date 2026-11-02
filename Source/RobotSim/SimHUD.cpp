#include "SimHUD.h"
#include "SimGameMode.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"

void ASimHUD::DrawHUD()
{
    Super::DrawHUD();
    auto* Sim = Cast<ASimGameMode>(GetWorld()->GetAuthGameMode());
    if (!Canvas || !Sim) return;
    DrawRect(FLinearColor::Black, 0, 0, Canvas->SizeX, Canvas->SizeY);
    const float W = FMath::Min(static_cast<float>(Canvas->SizeX),
                                Canvas->SizeY * 16.0f / 9.0f);
    const float H = W * 9.0f / 16.0f;
    if (auto* Texture = Sim->DisplayTexture())
        DrawTexture(Texture, (Canvas->SizeX - W) / 2, (Canvas->SizeY - H) / 2,
                    W, H, 0, 0, 1, 1, FLinearColor::White, BLEND_Opaque);
    const FString Info = Sim->Status();
    if (!Info.IsEmpty()) {
        TArray<FString> Lines;
        Info.ParseIntoArrayLines(Lines, false);
        for (int32 I = 0; I < Lines.Num(); ++I)
            DrawText(Lines[I], FLinearColor::Yellow, 16, 16 + 24 * I, nullptr, 1.0f);
    }
}
