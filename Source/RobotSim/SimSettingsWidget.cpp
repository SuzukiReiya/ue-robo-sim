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
    SetIsFocusable(true);
    if (WidgetTree->RootWidget) return;
    auto* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
    WidgetTree->RootWidget = Canvas;
    auto* Panel = WidgetTree->ConstructWidget<UBorder>();
    Panel->SetBrushColor(FLinearColor(0.035f, 0.05f, 0.08f, 0.98f));
    Panel->SetPadding(FMargin(20));
    // UWidget::Slotと名前が衝突しないよう、ローカル変数名を明確化する。
    auto* CanvasSlot = Canvas->AddChildToCanvas(Panel);
    CanvasSlot->SetAnchors(FAnchors(0.5f, 0.5f));
    CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
    CanvasSlot->SetSize(FVector2D(870, 640));
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

    // よく触る4項目を最上部へ。スライダーと右側の数値入力は同期する。
    Contents->AddChildToVerticalBox(MakeLabel(TEXT("■ 基本設定"), 17))
        ->SetPadding(FMargin(5, 10, 5, 3));
    const TCHAR* BasicTitles[] = {
        TEXT("移動感度（標準1.0）"),
        TEXT("旋回感度（標準1.0）"),
        TEXT("操作ラグ"),
        TEXT("映像ラグ")
    };
    const float BasicMin[] = {0.1f, 0.1f, 0.0f, 0.0f};
    const float BasicMax[] = {20.0f, 20.0f, 1000.0f, 1000.0f};
    for (int32 I = 0; I < 4; ++I) {
        auto* BasicRow = MakeRow(BasicTitles[I]);
        auto* Control = WidgetTree->ConstructWidget<USlider>();
        Control->SetMinValue(BasicMin[I]);
        Control->SetMaxValue(BasicMax[I]);
        Control->SetStepSize(I < 2 ? 0.1f : 10.0f);
        Control->SetSliderBarColor(FLinearColor(0.35f, 0.60f, 0.88f));
        Control->SetSliderHandleColor(FLinearColor(0.94f, 0.96f, 1.0f));
        auto* ControlSlot = BasicRow->AddChildToHorizontalBox(Control);
        ControlSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        ControlSlot->SetPadding(FMargin(12, 3, 12, 3));
        BasicSliders.Add(Control);

        auto* Numeric = WidgetTree->ConstructWidget<USpinBox>();
        Numeric->SetMinValue(BasicMin[I]);
        Numeric->SetMaxValue(BasicMax[I]);
        Numeric->SetMinSliderValue(BasicMin[I]);
        Numeric->SetMaxSliderValue(BasicMax[I]);
        Numeric->SetDelta(I < 2 ? 0.1f : 1.0f);
        Numeric->SetMinFractionalDigits(I < 2 ? 1 : 0);
        Numeric->SetMaxFractionalDigits(I < 2 ? 1 : 0);
        Numeric->SetMinDesiredWidth(100);
        Numeric->SetForegroundColor(FSlateColor(FLinearColor::Black));
        BasicRow->AddChildToHorizontalBox(Numeric)->SetSize(
            FSlateChildSize(ESlateSizeRule::Automatic));
        BasicNumbers.Add(Numeric);

        auto* Unit = MakeLabel(I < 2 ? TEXT("倍") : TEXT("ms"), 13);
        auto* UnitSlot = BasicRow->AddChildToHorizontalBox(Unit);
        UnitSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
        UnitSlot->SetVerticalAlignment(VAlign_Center);
        UnitSlot->SetPadding(FMargin(7, 0, 2, 0));
    }
    BasicSliders[0]->OnValueChanged.AddDynamic(this, &USimSettingsWidget::MoveSensitivityChanged);
    BasicSliders[1]->OnValueChanged.AddDynamic(this, &USimSettingsWidget::TurnSensitivityChanged);
    BasicSliders[2]->OnValueChanged.AddDynamic(this, &USimSettingsWidget::BasicCommandDelayChanged);
    BasicSliders[3]->OnValueChanged.AddDynamic(this, &USimSettingsWidget::BasicVideoDelayChanged);
    BasicNumbers[0]->OnValueCommitted.AddDynamic(this, &USimSettingsWidget::MoveSensitivityCommitted);
    BasicNumbers[1]->OnValueCommitted.AddDynamic(this, &USimSettingsWidget::TurnSensitivityCommitted);
    BasicNumbers[2]->OnValueCommitted.AddDynamic(this, &USimSettingsWidget::BasicCommandDelayCommitted);
    BasicNumbers[3]->OnValueCommitted.AddDynamic(this, &USimSettingsWidget::BasicVideoDelayCommitted);
    Contents->AddChildToVerticalBox(MakeLabel(
        TEXT("感度1.0倍＝移動150cm/秒・旋回120度/秒。20倍まで調整できます。"), 12))
        ->SetPadding(FMargin(5, 4));
    Contents->AddChildToVerticalBox(MakeLabel(
        TEXT("ラグはミリ秒で直接入力できます（0～1000ms）。"), 12))
        ->SetPadding(FMargin(5, 3));

    Contents->AddChildToVerticalBox(MakeLabel(TEXT("■ 操作割り当て"), 17))
        ->SetPadding(FMargin(5, 10, 5, 3));
    const TCHAR* Titles[] = {
        TEXT("前進"), TEXT("後退"), TEXT("左平行移動"), TEXT("右平行移動"),
        TEXT("左旋回"), TEXT("右旋回"), TEXT("射撃"),
        TEXT("砲身ピッチ上"), TEXT("砲身ピッチ下"), TEXT("照準補正（押している間）")
    };
    for (int32 I=0; I<10; ++I) {
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
    Combos[7]->OnSelectionChanged.AddDynamic(this, &USimSettingsWidget::PitchUpChanged);
    Combos[8]->OnSelectionChanged.AddDynamic(this, &USimSettingsWidget::PitchDownChanged);
    Combos[9]->OnSelectionChanged.AddDynamic(this, &USimSettingsWidget::AssistChanged);
    Contents->AddChildToVerticalBox(MakeLabel(
        TEXT("マウス上下＝砲身ピッチ、Shift長押し＝照準補正。割当変更可能"), 12))
        ->SetPadding(FMargin(5, 5));

    Contents->AddChildToVerticalBox(MakeLabel(TEXT("■ 詳細設定（物理・映像）"), 17))
        ->SetPadding(FMargin(5, 12, 5, 3));
    const TCHAR* OptionTitles[] = {
        TEXT("マウスボタンの移動／旋回入力強度"),
        TEXT("機体の慣性揺れ (度)"),
        TEXT("射撃ごとのブレ (度)"),
        TEXT("タイヤの滑り (0=滑らない、1=大きく滑る)"),
        TEXT("カメラ表示FPS"),
        TEXT("旋回慣性（ms、離した後の旋回の残り具合）"),
        TEXT("照準補正：パネル正面の許容角度（度）")
    };
    const float Low[] = {0.05, 0, 0, 0, 1, 0, 0};
    const float High[] = {1, 8, 8, 1, 60, 1500, 90};
    const float Delta[] = {0.05, 0.1, 0.1, 0.05, 1, 25, 1};
    for (int32 I=0; I<7; ++I) {
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
    Numbers[0]->OnValueChanged.AddDynamic(this, &USimSettingsWidget::MouseButtonChanged);
    Numbers[1]->OnValueChanged.AddDynamic(this, &USimSettingsWidget::BodySwayChanged);
    Numbers[2]->OnValueChanged.AddDynamic(this, &USimSettingsWidget::RecoilChanged);
    Numbers[3]->OnValueChanged.AddDynamic(this, &USimSettingsWidget::SlipChanged);
    Numbers[4]->OnValueChanged.AddDynamic(this, &USimSettingsWidget::DisplayFpsChanged);
    Numbers[5]->OnValueChanged.AddDynamic(this, &USimSettingsWidget::TurnInertiaChanged);
    Numbers[6]->OnValueChanged.AddDynamic(this, &USimSettingsWidget::AimFacingChanged);

    // 地表面から75°で固定する。パネル中心高さのみルール内の5パターンを選択。
    auto* PanelRow = MakeRow(TEXT("敵機ヒットパネルの高さ配置"));
    PanelProfileCombo = WidgetTree->ConstructWidget<UComboBoxString>();
    PanelProfileCombo->SetItemStyle(Combos[0]->GetItemStyle());
    PanelProfileCombo->ForegroundColor = FSlateColor(FLinearColor::White);
    PanelProfileCombo->SetContentPadding(FMargin(9,5));
    PanelProfileCombo->AddOption(TEXT("低: 6cm（四方向）"));
    PanelProfileCombo->AddOption(TEXT("標準: 9cm（四方向）"));
    PanelProfileCombo->AddOption(TEXT("高: 12cm（四方向）"));
    PanelProfileCombo->AddOption(TEXT("上限付近: 14.5cm（四方向）"));
    PanelProfileCombo->AddOption(TEXT("段違い: 前後12cm・左右6cm"));
    PanelRow->AddChildToHorizontalBox(PanelProfileCombo)->SetSize(
        FSlateChildSize(ESlateSizeRule::Fill));
    PanelProfileCombo->OnSelectionChanged.AddDynamic(
        this, &USimSettingsWidget::PanelProfileChanged);
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
    if (Combos.Num() != 10 || Numbers.Num() != 7 || BasicNumbers.Num() != 4 ||
        BasicSliders.Num() != 4 || !PanelProfileCombo) return;
    bUpdating = true;
    const RobotSim::InputSource Keys[] = {
        Working.forward,
        Working.backward,
        Working.strafeLeft,
        Working.strafeRight,
        Working.turnLeft,
        Working.turnRight,
        Working.fire,
        Working.pitchUp,
        Working.pitchDown,
        Working.aimAssist
    };
    for (int32 I=0; I<10; ++I)
        Combos[I]->SetSelectedOption(UTF8_TO_TCHAR(RobotSim::sourceToken(Keys[I])));
    const double BasicValues[] = {
        WorkingTuning.moveSensitivity, WorkingTuning.turnSensitivity,
        WorkingTuning.commandDelayMs, WorkingTuning.videoDelayMs
    };
    for (int32 I = 0; I < 4; ++I) {
        BasicSliders[I]->SetValue(static_cast<float>(BasicValues[I]));
        BasicNumbers[I]->SetValue(static_cast<float>(BasicValues[I]));
    }
    const double Values[] = {
        WorkingTuning.mouseButtonStrength,
        WorkingTuning.bodySwayDegrees,
        WorkingTuning.shotRecoilDegrees,
        WorkingTuning.tireSlip,
        WorkingTuning.displayFps,
        WorkingTuning.turnInertiaMs,
        WorkingTuning.aimFacingAngleDeg
    };
    for (int32 I=0; I<7; ++I)
        Numbers[I]->SetValue(static_cast<float>(Values[I]));
    PanelProfileCombo->SetSelectedOption(
        PanelProfileCombo->GetOptionAtIndex(WorkingTuning.panelPattern));
    bUpdating = false;
    UpdateSaveState();
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
    case 7: Working.pitchUp = Key; break;
    case 8: Working.pitchDown = Key; break;
    case 9: Working.aimAssist = Key; break;
    default: return;
    }
    Refresh();
}
void USimSettingsWidget::OnNumber(int32 Index, float Value)
{
    if (bUpdating) return;
    switch (Index) {
    case 0: WorkingTuning.mouseButtonStrength = Value; break;
    case 1: WorkingTuning.bodySwayDegrees = Value; break;
    case 2: WorkingTuning.shotRecoilDegrees = Value; break;
    case 3: WorkingTuning.tireSlip = Value; break;
    case 4: WorkingTuning.displayFps = Value; break;
    case 5: WorkingTuning.turnInertiaMs = Value; break;
    case 6: WorkingTuning.aimFacingAngleDeg = Value; break;
    default: return;
    }
    // 編集中にスピン値を書き戻すと入力欄がリセットされるので状態表示のみ更新。
    const bool bValid = Working.valid() && WorkingTuning.valid();
    if (SaveButton) SaveButton->SetIsEnabled(bValid);
}
void USimSettingsWidget::UpdateSaveState()
{
    const bool bValid = Working.valid() && WorkingTuning.valid();
    if (SaveButton) SaveButton->SetIsEnabled(bValid);
    if (Message) Message->SetText(FText::FromString(bValid
        ? TEXT("設定値は有効です。保存すると次回起動時にも復元されます。")
        : TEXT("キーの重複、または数値範囲を確認してください。")));
}
void USimSettingsWidget::OnBasicNumber(int32 Index, float Value)
{
    if (bUpdating || Index < 0 || Index >= BasicNumbers.Num() ||
        Index >= BasicSliders.Num() || !FMath::IsFinite(Value)) return;
    // スライダーは感度0.1倍刻み、ラグは1ms刻み。数値直接入力も同じ規則。
    const double Min = Index < 2 ? 0.1 : 0.0;
    const double Max = Index < 2 ? 20.0 : 1000.0;
    const double Clamped = FMath::Clamp(static_cast<double>(Value), Min, Max);
    const double Normalized = Index < 2
        ? static_cast<double>(FMath::RoundToInt(Clamped * 10.0)) / 10.0
        : static_cast<double>(FMath::RoundToInt(Clamped));
    switch (Index) {
    case 0: WorkingTuning.moveSensitivity = Normalized; break;
    case 1: WorkingTuning.turnSensitivity = Normalized; break;
    case 2: WorkingTuning.commandDelayMs = Normalized; break;
    case 3: WorkingTuning.videoDelayMs = Normalized; break;
    default: return;
    }
    bUpdating = true;
    BasicSliders[Index]->SetValue(static_cast<float>(Normalized));
    BasicNumbers[Index]->SetValue(static_cast<float>(Normalized));
    bUpdating = false;
    UpdateSaveState();
}
void USimSettingsWidget::ForwardChanged(FString V, ESelectInfo::Type) { OnSelection(0, V); }
void USimSettingsWidget::BackwardChanged(FString V, ESelectInfo::Type) { OnSelection(1, V); }
void USimSettingsWidget::StrafeLeftChanged(FString V, ESelectInfo::Type) { OnSelection(2, V); }
void USimSettingsWidget::StrafeRightChanged(FString V, ESelectInfo::Type) { OnSelection(3, V); }
void USimSettingsWidget::LeftChanged(FString V, ESelectInfo::Type) { OnSelection(4, V); }
void USimSettingsWidget::RightChanged(FString V, ESelectInfo::Type) { OnSelection(5, V); }
void USimSettingsWidget::FireChanged(FString V, ESelectInfo::Type) { OnSelection(6, V); }
void USimSettingsWidget::PitchUpChanged(FString V, ESelectInfo::Type) { OnSelection(7, V); }
void USimSettingsWidget::PitchDownChanged(FString V, ESelectInfo::Type) { OnSelection(8, V); }
void USimSettingsWidget::AssistChanged(FString V, ESelectInfo::Type) { OnSelection(9, V); }
void USimSettingsWidget::MouseButtonChanged(float V) { OnNumber(0, V); }
void USimSettingsWidget::BodySwayChanged(float V) { OnNumber(1, V); }
void USimSettingsWidget::RecoilChanged(float V) { OnNumber(2, V); }
void USimSettingsWidget::SlipChanged(float V) { OnNumber(3, V); }
void USimSettingsWidget::DisplayFpsChanged(float V) { OnNumber(4, V); }
void USimSettingsWidget::TurnInertiaChanged(float V) { OnNumber(5, V); }
void USimSettingsWidget::AimFacingChanged(float V) { OnNumber(6, V); }
void USimSettingsWidget::MoveSensitivityChanged(float V) { OnBasicNumber(0, V); }
void USimSettingsWidget::TurnSensitivityChanged(float V) { OnBasicNumber(1, V); }
void USimSettingsWidget::BasicCommandDelayChanged(float V) { OnBasicNumber(2, V); }
void USimSettingsWidget::BasicVideoDelayChanged(float V) { OnBasicNumber(3, V); }
void USimSettingsWidget::MoveSensitivityCommitted(float V, ETextCommit::Type) { OnBasicNumber(0, V); }
void USimSettingsWidget::TurnSensitivityCommitted(float V, ETextCommit::Type) { OnBasicNumber(1, V); }
void USimSettingsWidget::BasicCommandDelayCommitted(float V, ETextCommit::Type) { OnBasicNumber(2, V); }
void USimSettingsWidget::BasicVideoDelayCommitted(float V, ETextCommit::Type) { OnBasicNumber(3, V); }
void USimSettingsWidget::PanelProfileChanged(FString Value, ESelectInfo::Type)
{
    if (bUpdating || !PanelProfileCombo) return;
    for (int32 I=0; I<RobotSim::PanelPatternCount; ++I) {
        if (PanelProfileCombo->GetOptionAtIndex(I) == Value) {
            WorkingTuning.panelPattern = I;
            break;
        }
    }
    const bool bValid = Working.valid() && WorkingTuning.valid();
    if (SaveButton) SaveButton->SetIsEnabled(bValid);
}

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
