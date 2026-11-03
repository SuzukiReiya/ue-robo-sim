#include "SimSettingsWidget.h"
#include "SimGameMode.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
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
    Panel->SetBrushColor(FLinearColor(0.035f, 0.05f, 0.08f, 0.98f));
    Panel->SetPadding(FMargin(20));
    auto* Slot = Canvas->AddChildToCanvas(Panel);
    Slot->SetAnchors(FAnchors(0.5f, 0.5f));
    Slot->SetAlignment(FVector2D(0.5f, 0.5f));
    Slot->SetSize(FVector2D(870, 640));
    auto* Column = WidgetTree->ConstructWidget<UVerticalBox>();
    Panel->SetContent(Column);

    auto MakeLabel = [this](const FString& Caption, int32 Size) {
        auto* Label = WidgetTree->ConstructWidget<UTextBlock>();
        Label->SetText(FText::FromString(Caption));
        Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
        FSlateFontInfo Font = Label->GetFont();
        Font.Size = Size;
        Label->SetFont(Font);
        return Label;
    };
    Column->AddChildToVerticalBox(MakeLabel(TEXT("一時停止 / 操作・シミュレーション設定"), 23))
        ->SetPadding(FMargin(4, 5));
    Column->AddChildToVerticalBox(MakeLabel(
        TEXT("項目を変更後、保存して再開。重複割り当ては不可。「なし」は全操作で利用可能です。"), 13))
        ->SetPadding(FMargin(4, 4));
    auto* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
    Scroll->SetScrollBarVisibility(ESlateVisibility::Visible);
    Column->AddChildToVerticalBox(Scroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    auto* Contents = WidgetTree->ConstructWidget<UVerticalBox>();
    Scroll->AddChild(Contents);
    auto MakeRow = [this, Contents, MakeLabel](const FString& Caption) {
        auto* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
        Contents->AddChildToVerticalBox(Row)->SetPadding(FMargin(5, 4));
        auto* Label = MakeLabel(Caption, 14);
        auto* L = Row->AddChildToHorizontalBox(Label);
        L->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        L->SetVerticalAlignment(VAlign_Center);
        return Row;
    };

    Contents->AddChildToVerticalBox(MakeLabel(TEXT("■ 操作割り当て"), 17))
        ->SetPadding(FMargin(5, 10, 5, 3));
    const TCHAR* Titles[] = {
        TEXT("前進"), TEXT("後退"), TEXT("左平行移動"), TEXT("右平行移動"),
        TEXT("左旋回"), TEXT("右旋回"), TEXT("射撃")
    };
    for (int32 I=0; I<7; ++I) {
        auto* Row = MakeRow(Titles[I]);
        auto* Combo = WidgetTree->ConstructWidget<UComboBoxString>();
        // 既定テーマの暗背景に黒文字が重なるのを防ぐ。
        Combo->ForegroundColor = FSlateColor(FLinearColor::White);
        FTableRowStyle RowStyle = Combo->GetItemStyle();
        RowStyle.SetTextColor(FSlateColor(FLinearColor::White));
        RowStyle.SetSelectedTextColor(FSlateColor(FLinearColor::White));
        Combo->SetItemStyle(RowStyle);
        Combo->SetContentPadding(FMargin(9, 5));
        Combo->SetMaxListHeight(250);
        for (std::size_t K=0; K<RobotSim::SourceCount; ++K)
            Combo->AddOption(UTF8_TO_TCHAR(RobotSim::sourceToken(
                static_cast<RobotSim::InputSource>(K))));
        Row->AddChildToHorizontalBox(Combo)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        Combos.Add(Combo);
    }
    Combos[0]->OnSelectionChanged.AddDynamic(this, &USimSettingsWidget::ForwardChanged);
    Combos[1]->OnSelectionChanged.AddDynamic(this, &USimSettingsWidget::BackwardChanged);
    Combos[2]->OnSelectionChanged.AddDynamic(this, &USimSettingsWidget::StrafeLeftChanged);
    Combos[3]->OnSelectionChanged.AddDynamic(this, &USimSettingsWidget::StrafeRightChanged);
    Combos[4]->OnSelectionChanged.AddDynamic(this, &USimSettingsWidget::LeftChanged);
    Combos[5]->OnSelectionChanged.AddDynamic(this, &USimSettingsWidget::RightChanged);
    Combos[6]->OnSelectionChanged.AddDynamic(this, &USimSettingsWidget::FireChanged);
    Contents->AddChildToVerticalBox(MakeLabel(
        TEXT("MouseLeft/Right/Up/Down＝マウス移動方向、MouseLMB/RMB＝クリック"), 12))
        ->SetPadding(FMargin(5, 5));

    Contents->AddChildToVerticalBox(MakeLabel(TEXT("■ 操作感度・機体・映像"), 17))
        ->SetPadding(FMargin(5, 12, 5, 3));
    const TCHAR* OptionTitles[] = {
        TEXT("マウス左右の操作感度 (指令/px)"),
        TEXT("マウス上下の操作感度 (指令/px)"),
        TEXT("マウスボタンの操作強度 (0.05～1)"),
        TEXT("機体の慣性揺れ (度)"),
        TEXT("射撃ごとのブレ (度)"),
        TEXT("タイヤの滑り (0=滑らない、1=大きく滑る)"),
        TEXT("操作指令ラグ (ms)"),
        TEXT("映像表示ラグ (ms)"),
        TEXT("カメラ表示FPS")
    };
    const float Low[] = {0.001, 0.001, 0.05, 0, 0, 0, 0, 0, 1};
    const float High[] = {1, 1, 1, 8, 8, 1, 1000, 1000, 60};
    const float Delta[] = {0.01, 0.01, 0.05, 0.1, 0.1, 0.05, 10, 10, 1};
    for (int32 I=0; I<9; ++I) {
        auto* Row = MakeRow(OptionTitles[I]);
        auto* Spin = WidgetTree->ConstructWidget<USpinBox>();
        Spin->SetMinValue(Low[I]);
        Spin->SetMaxValue(High[I]);
        Spin->SetMinSliderValue(Low[I]);
        Spin->SetMaxSliderValue(High[I]);
        Spin->SetDelta(Delta[I]);
        Spin->SetMinDesiredWidth(170);
        Spin->SetForegroundColor(FSlateColor(FLinearColor::Black));
        Row->AddChildToHorizontalBox(Spin)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        Numbers.Add(Spin);
    }
    Numbers[0]->OnValueChanged.AddDynamic(this, &USimSettingsWidget::MouseHorizontalChanged);
    Numbers[1]->OnValueChanged.AddDynamic(this, &USimSettingsWidget::MouseVerticalChanged);
    Numbers[2]->OnValueChanged.AddDynamic(this, &USimSettingsWidget::MouseButtonChanged);
    Numbers[3]->OnValueChanged.AddDynamic(this, &USimSettingsWidget::BodySwayChanged);
    Numbers[4]->OnValueChanged.AddDynamic(this, &USimSettingsWidget::RecoilChanged);
    Numbers[5]->OnValueChanged.AddDynamic(this, &USimSettingsWidget::SlipChanged);
    Numbers[6]->OnValueChanged.AddDynamic(this, &USimSettingsWidget::CommandDelayChanged);
    Numbers[7]->OnValueChanged.AddDynamic(this, &USimSettingsWidget::VideoDelayChanged);
    Numbers[8]->OnValueChanged.AddDynamic(this, &USimSettingsWidget::DisplayFpsChanged);
    Contents->AddChildToVerticalBox(MakeLabel(
        TEXT("注意: 慣性揺れ・射撃反動はカメラと射線に影響します。"), 12))
        ->SetPadding(FMargin(5, 9));
    Contents->AddChildToVerticalBox(MakeLabel(
        TEXT("Esc / Space / 1～3 / F1 は予約キー。遅延値・FPSは実験条件です。"), 12))
        ->SetPadding(FMargin(5, 5));

    Message = MakeLabel(TEXT(""), 13);
    Column->AddChildToVerticalBox(Message)->SetPadding(FMargin(5, 7));
    auto* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>();
    Column->AddChildToVerticalBox(Buttons)->SetPadding(FMargin(5, 8));
    auto MakeButton = [this, Buttons, MakeLabel](const TCHAR* Caption) {
        auto* Button = WidgetTree->ConstructWidget<UButton>();
        Button->AddChild(MakeLabel(Caption, 13));
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

void USimSettingsWidget::SetSettings(const RobotSim::InputBindings& Current,
                                     const RobotSim::Tuning& Tuning)
{
    Working = Current;
    WorkingTuning = Tuning;
    Refresh();
}

void USimSettingsWidget::Refresh()
{
    if (Combos.Num() != 7 || Numbers.Num() != 9) return;
    bUpdating = true;
    const RobotSim::InputSource Keys[] = {
        Working.forward,
        Working.backward,
        Working.strafeLeft,
        Working.strafeRight,
        Working.turnLeft,
        Working.turnRight,
        Working.fire
    };
    for (int32 I=0; I<7; ++I)
        Combos[I]->SetSelectedOption(UTF8_TO_TCHAR(RobotSim::sourceToken(Keys[I])));
    const double Values[] = {
        WorkingTuning.mouseHorizontal,
        WorkingTuning.mouseVertical,
        WorkingTuning.mouseButtonStrength,
        WorkingTuning.bodySwayDegrees,
        WorkingTuning.shotRecoilDegrees,
        WorkingTuning.tireSlip,
        WorkingTuning.commandDelayMs,
        WorkingTuning.videoDelayMs,
        WorkingTuning.displayFps
    };
    for (int32 I=0; I<9; ++I)
        Numbers[I]->SetValue(static_cast<float>(Values[I]));
    bUpdating = false;
    const bool bValid = Working.valid() && WorkingTuning.valid();
    if (SaveButton) SaveButton->SetIsEnabled(bValid);
    if (Message) Message->SetText(FText::FromString(bValid
        ? TEXT("設定値は有効です。保存すると再起動後も復元されます。")
        : TEXT("キーの重複または数値範囲を確認してください。")));
}

void USimSettingsWidget::OnSelection(int32 Index, const FString& Value)
{
    if (bUpdating) return;
    RobotSim::InputSource Key;
    if (!RobotSim::parseSource(TCHAR_TO_UTF8(*Value), Key)) return;
    switch (Index) {
    case 0: Working.forward = Key; break;
    case 1: Working.backward = Key; break;
    case 2: Working.strafeLeft = Key; break;
    case 3: Working.strafeRight = Key; break;
    case 4: Working.turnLeft = Key; break;
    case 5: Working.turnRight = Key; break;
    case 6: Working.fire = Key; break;
    default: return;
    }
    Refresh();
}
void USimSettingsWidget::OnNumber(int32 Index, float Value)
{
    if (bUpdating) return;
    switch (Index) {
    case 0: WorkingTuning.mouseHorizontal = Value; break;
    case 1: WorkingTuning.mouseVertical = Value; break;
    case 2: WorkingTuning.mouseButtonStrength = Value; break;
    case 3: WorkingTuning.bodySwayDegrees = Value; break;
    case 4: WorkingTuning.shotRecoilDegrees = Value; break;
    case 5: WorkingTuning.tireSlip = Value; break;
    case 6: WorkingTuning.commandDelayMs = Value; break;
    case 7: WorkingTuning.videoDelayMs = Value; break;
    case 8: WorkingTuning.displayFps = Value; break;
    default: return;
    }
    // 編集中にスピン値を書き戻すと入力欄がリセットされるので状態表示のみ更新。
    const bool bValid = Working.valid() && WorkingTuning.valid();
    if (SaveButton) SaveButton->SetIsEnabled(bValid);
}
void USimSettingsWidget::ForwardChanged(FString V, ESelectInfo::Type) { OnSelection(0, V); }
void USimSettingsWidget::BackwardChanged(FString V, ESelectInfo::Type) { OnSelection(1, V); }
void USimSettingsWidget::StrafeLeftChanged(FString V, ESelectInfo::Type) { OnSelection(2, V); }
void USimSettingsWidget::StrafeRightChanged(FString V, ESelectInfo::Type) { OnSelection(3, V); }
void USimSettingsWidget::LeftChanged(FString V, ESelectInfo::Type) { OnSelection(4, V); }
void USimSettingsWidget::RightChanged(FString V, ESelectInfo::Type) { OnSelection(5, V); }
void USimSettingsWidget::FireChanged(FString V, ESelectInfo::Type) { OnSelection(6, V); }
void USimSettingsWidget::MouseHorizontalChanged(float V) { OnNumber(0, V); }
void USimSettingsWidget::MouseVerticalChanged(float V) { OnNumber(1, V); }
void USimSettingsWidget::MouseButtonChanged(float V) { OnNumber(2, V); }
void USimSettingsWidget::BodySwayChanged(float V) { OnNumber(3, V); }
void USimSettingsWidget::RecoilChanged(float V) { OnNumber(4, V); }
void USimSettingsWidget::SlipChanged(float V) { OnNumber(5, V); }
void USimSettingsWidget::CommandDelayChanged(float V) { OnNumber(6, V); }
void USimSettingsWidget::VideoDelayChanged(float V) { OnNumber(7, V); }
void USimSettingsWidget::DisplayFpsChanged(float V) { OnNumber(8, V); }

void USimSettingsWidget::SaveAndResume()
{
    auto* Sim = GetWorld() ? GetWorld()->GetAuthGameMode<ASimGameMode>() : nullptr;
    if (Sim && Sim->ApplySettings(Working, WorkingTuning))
        Sim->SetSimulationPaused(false);
    else if (Message) Message->SetText(FText::FromString(TEXT("設定の保存に失敗しました。")));
}
void USimSettingsWidget::Resume()
{
    if (auto* Sim = GetWorld()->GetAuthGameMode<ASimGameMode>())
        Sim->SetSimulationPaused(false);
}
void USimSettingsWidget::Reset()
{
    Working = RobotSim::InputBindings{};
    WorkingTuning = RobotSim::Tuning{};
    Refresh();
}
void USimSettingsWidget::ExitGame()
{
    if (auto* PC=GetOwningPlayer()) PC->ConsoleCommand(TEXT("quit"));
}
FReply USimSettingsWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    if (Event.GetKey() == EKeys::Escape) {
        Resume();
        return FReply::Handled();
    }
    return Super::NativeOnKeyDown(Geometry, Event);
}
