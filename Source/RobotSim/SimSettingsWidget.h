#pragma once
#include "Blueprint/UserWidget.h"
#include "Components/ComboBoxString.h"
#include "Components/SpinBox.h"
#include "SimInputBindings.h"
#include "SimSettingsWidget.generated.h"

class UButton;
class UTextBlock;

// 7種類の操作割り当てと物理・映像条件を一画面で設定する。
UCLASS()
class ROBOTSIM_API USimSettingsWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void SetSettings(const RobotSim::InputBindings& Current,
                     const RobotSim::Tuning& Tuning);
protected:
    virtual void NativeOnInitialized() override;
    virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
private:
    RobotSim::InputBindings Working;
    RobotSim::Tuning WorkingTuning;
    UPROPERTY() TArray<TObjectPtr<UComboBoxString>> Combos;
    UPROPERTY() TArray<TObjectPtr<USpinBox>> Numbers;
    UPROPERTY() TObjectPtr<UComboBoxString> PanelProfileCombo;
    UPROPERTY() TObjectPtr<UTextBlock> Message;
    UPROPERTY() TObjectPtr<UButton> SaveButton;
    bool bUpdating = false;
    void OnSelection(int32 Index, const FString& Value);
    void OnNumber(int32 Index, float Value);
    void Refresh();
    UFUNCTION() void ForwardChanged(FString Value, ESelectInfo::Type Selection);
    UFUNCTION() void BackwardChanged(FString Value, ESelectInfo::Type Selection);
    UFUNCTION() void StrafeLeftChanged(FString Value, ESelectInfo::Type Selection);
    UFUNCTION() void StrafeRightChanged(FString Value, ESelectInfo::Type Selection);
    UFUNCTION() void LeftChanged(FString Value, ESelectInfo::Type Selection);
    UFUNCTION() void RightChanged(FString Value, ESelectInfo::Type Selection);
    UFUNCTION() void FireChanged(FString Value, ESelectInfo::Type Selection);
    UFUNCTION() void MouseHorizontalChanged(float Value);
    UFUNCTION() void MouseVerticalChanged(float Value);
    UFUNCTION() void MouseButtonChanged(float Value);
    UFUNCTION() void BodySwayChanged(float Value);
    UFUNCTION() void RecoilChanged(float Value);
    UFUNCTION() void SlipChanged(float Value);
    UFUNCTION() void CommandDelayChanged(float Value);
    UFUNCTION() void VideoDelayChanged(float Value);
    UFUNCTION() void DisplayFpsChanged(float Value);
    UFUNCTION() void TurnInertiaChanged(float Value);
    UFUNCTION() void PanelProfileChanged(FString Value, ESelectInfo::Type Selection);
    UFUNCTION() void SaveAndResume();
    UFUNCTION() void Resume();
    UFUNCTION() void Reset();
    UFUNCTION() void ExitGame();
};
