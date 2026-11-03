#pragma once
#include "Blueprint/UserWidget.h"
#include "Components/ComboBoxString.h"
#include "SimInputBindings.h"
#include "SimSettingsWidget.generated.h"

class UButton;
class UTextBlock;

// Escで開く設定画面。変更中はゲーム内入力へ伝播しない。
UCLASS()
class ROBOTSIM_API USimSettingsWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void SetBindings(const RobotSim::InputBindings& Current);
protected:
    virtual void NativeConstruct() override;
    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
private:
    RobotSim::InputBindings Working;
    UPROPERTY() TArray<TObjectPtr<UComboBoxString>> Combos;
    UPROPERTY() TObjectPtr<UTextBlock> Message;
    UPROPERTY() TObjectPtr<UButton> SaveButton;
    bool bUpdating = false;
    void OnSelection(int32 Index, const FString& Value);
    void Refresh();
    UFUNCTION() void ForwardChanged(FString Value, ESelectInfo::Type Selection);
    UFUNCTION() void BackwardChanged(FString Value, ESelectInfo::Type Selection);
    UFUNCTION() void LeftChanged(FString Value, ESelectInfo::Type Selection);
    UFUNCTION() void RightChanged(FString Value, ESelectInfo::Type Selection);
    UFUNCTION() void FireChanged(FString Value, ESelectInfo::Type Selection);
    UFUNCTION() void SaveAndResume();
    UFUNCTION() void Resume();
    UFUNCTION() void Reset();
    UFUNCTION() void ExitGame();
};
