#pragma once
#include "CoreMinimal.h"
#include "SimHitPanelConfig.h"

class AActor;
class USceneComponent;

// 敵機の四方へ固定したパネルを取り付ける。操作・射撃・真値を一切参照しない。
class FSimHitPanels
{
public:
    void Initialize(AActor* Target, int Pattern);
    void SetPattern(int Pattern);
    int Pattern() const { return CurrentPattern; }
private:
    TWeakObjectPtr<AActor> Owner;
    TArray<TWeakObjectPtr<USceneComponent>> Anchors;
    int CurrentPattern = 1;
};
