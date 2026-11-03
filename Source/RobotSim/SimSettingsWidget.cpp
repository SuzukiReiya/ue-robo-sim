#include "SimSettingsWidget.h"
#include "SimGameMode.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Input/Reply.h"

void USimSettingsWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    bIsFocusable = true;
    if (WidgetTree->RootWidget) return;

    auto* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
    WidgetTree->RootWidget = Canvas;
    auto* Panel = WidgetTree->ConstructWidget<UBorder>();
    Panel->SetBrushColor(FLinearColor(0.04f, 0.06f, 0.09f, 0.97f));
    Panel->SetPadding(FMargin(22.0f));
    auto* CanvasSlot = Canvas->AddChildToCanvas(Panel);
    CanvasSlot->SetAnchors(FAnchors(0.5f, 0.5f));
    CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
    CanvasSlot->SetSize(FVector2D(700, 525));

    auto* Column = WidgetTree->ConstructWidget<UVerticalBox>();
    Panel->SetContent(Column);
    auto AddText = [this, Column](const FString& Value, int32 FontSize) {
        auto* Label = WidgetTree->ConstructWidget<UTextBlock>();
        Label->SetText(FText::FromString(Value));
        Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
        FSlateFontInfo Font = Label->GetFont();
        Font.Size = FontSize;
        Label->SetFont(Font);
        Column->AddChildToVerticalBox(Label)->SetPadding(FMargin(4, 7));
        return Label;
    };
    AddText(TEXT("一時停止 / 操作設定"), 26);
    AddText(TEXT("割り当てを選び「保存して再開」で確定。Esc: 保存せず再開"), 14);

    const TCHAR* Titles[] = {
        TEXT("前進"), TEXT("後退"), TEXT("左旋回"), TEXT("右旋回"), TEXT("射撃")
    };
    for (int32 I = 0; I < 5; ++I) {
        auto* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
        Column->AddChildToVerticalBox(Row)->SetPadding(FMargin(5, 5));
        auto* Label = WidgetTree->ConstructWidget<UTextBlock>();
        Label->SetText(FText::FromString(Titles[I]));
        Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
        auto* LabelSlot = Row->AddChildToHorizontalBox(Label);
        LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        LabelSlot->SetVerticalAlignment(VAlign_Center);
        auto* Combo = WidgetTree->ConstructWidget<UComboBoxString>();
        for (std::size_t K=0; K<RobotSim::SourceCount; ++K)
            Combo->AddOption(UTF8_TO_TCHAR(RobotSim::sourceToken(
                static_cast<RobotSim::InputSource>(K))));
        Row->AddChildToHorizontalBox(Combo)->SetSize(
            FSlateChildSize(ESlateSizeRule::Fill));
        Combos.Add(Combo);
    }
    Combos[0]->OnSelectionChanged.AddDynamic(this, &USimSettingsWidget::ForwardChanged);
    Combos[1]->OnSelectionChanged.AddDynamic(this, &USimSettingsWidget::BackwardChanged);
    Combos[2]->OnSelectionChanged.AddDynamic(this, &USimSettingsWidget::LeftChanged);
    Combos[3]->OnSelectionChanged.AddDynamic(this, &USimSettingsWidget::RightChanged);
    Combos[4]->OnSelectionChanged.AddDynamic(this, &USimSettingsWidget::FireChanged);

    Message = AddText(TEXT(""), 14);
    AddText(TEXT("MouseUp/Down/Left/Right: マウス移動方向、MouseLMB/RMB: 左右クリック"), 12);
    AddText(TEXT("Esc・Space・1/2/3・F1は予約キーです。"), 12);

    auto* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>();
    Column->AddChildToVerticalBox(Buttons)->SetPadding(FMargin(5, 12));
    auto MakeButton = [this, Buttons](const TCHAR* Caption) {
        auto* Button = WidgetTree->ConstructWidget<UButton>();
        auto* Text = WidgetTree->ConstructWidget<UTextBlock>();
        Text->SetText(FText::FromString(Caption));
        Button->AddChild(Text);
        Buttons->AddChildToHorizontalBox(Button)->SetPadding(FMargin(5));
        return Button;
    };
    SaveButton = MakeButton(TEXT("保存して再開"));
    SaveButton->OnClicked.AddDynamic(this, &USimSettingsWidget::SaveAndResume);
    MakeButton(TEXT("保存せず再開"))->OnClicked.AddDynamic(this, &USimSettingsWidget::Resume);
    MakeButton(TEXT("初期値に戻す"))->OnClicked.AddDynamic(this, &USimSettingsWidget::Reset);
    MakeButton(TEXT("終了"))->OnClicked.AddDynamic(this, &USimSettingsWidget::ExitGame);
    Refresh();
}

void USimSettingsWidget::SetBindings(const RobotSim::InputBindings& Current)
{
    Working = Current;
    Refresh();
}

void USimSettingsWidget::Refresh()
{
    if (Combos.Num() != 5) return;
    bUpdating = true;
    const RobotSim::InputSource Keys[] = {
        Working.forward, Working.backward, Working.turnLeft, Working.turnRight, Working.fire
    };
    for (int32 I=0; I<5; ++I)
        Combos[I]->SetSelectedOption(UTF8_TO_TCHAR(RobotSim::sourceToken(Keys[I])));
    bUpdating = false;
    const bool bValid = Working.valid();
    if (SaveButton) SaveButton->SetIsEnabled(bValid);
    if (Message) Message->SetText(FText::FromString(bValid
        ? TEXT("重複のない割り当てです。")
        : TEXT("割り当ての重複、または移動操作の「なし」を修正してください。")));
}

void USimSettingsWidget::OnSelection(int32 Index, const FString& Value)
{
    if (bUpdating) return;
    RobotSim::InputSource Key;
    if (!RobotSim::parseSource(TCHAR_TO_UTF8(*Value), Key)) return;
    switch (Index) {
    case 0: Working.forward = Key; break;
    case 1: Working.backward = Key; break;
    case 2: Working.turnLeft = Key; break;
    case 3: Working.turnRight = Key; break;
    case 4: Working.fire = Key; break;
    default: return;
    }
    Refresh();
}
void USimSettingsWidget::ForwardChanged(FString V, ESelectInfo::Type) { OnSelection(0, V); }
void USimSettingsWidget::BackwardChanged(FString V, ESelectInfo::Type) { OnSelection(1, V); }
void USimSettingsWidget::LeftChanged(FString V, ESelectInfo::Type) { OnSelection(2, V); }
void USimSettingsWidget::RightChanged(FString V, ESelectInfo::Type) { OnSelection(3, V); }
void USimSettingsWidget::FireChanged(FString V, ESelectInfo::Type) { OnSelection(4, V); }

void USimSettingsWidget::SaveAndResume()
{
    auto* Sim = GetWorld() ? GetWorld()->GetAuthGameMode<ASimGameMode>() : nullptr;
    if (Sim && Sim->ApplyInputBindings(Working))
        Sim->SetSimulationPaused(false);
    else if (Message)
        Message->SetText(FText::FromString(TEXT("設定の保存に失敗しました。")));
}
void USimSettingsWidget::Resume()
{
    if (auto* Sim = GetWorld()->GetAuthGameMode<ASimGameMode>())
        Sim->SetSimulationPaused(false);
}
void USimSettingsWidget::Reset()
{
    Working = RobotSim::InputBindings{};
    Refresh();
}
void USimSettingsWidget::ExitGame()
{
    if (auto* PC = GetOwningPlayer()) PC->ConsoleCommand(TEXT("quit"));
}
FReply USimSettingsWidget::NativeOnKeyDown(
    const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    if (InKeyEvent.GetKey() == EKeys::Escape) {
        Resume();
        return FReply::Handled();
    }
    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}
