#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "SimHUD.generated.h"

// 操縦者用の画面描画だけを担当。指令生成・物理・撮影を行わない。
UCLASS()
class ROBOTSIM_API ASimHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
};
