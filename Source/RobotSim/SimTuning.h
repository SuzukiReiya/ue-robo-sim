#pragma once
#include <cmath>
#include "SimHitPanelConfig.h"

namespace RobotSim {
// 実験条件。大きい値が何を意味するかを各パラメータ名で明確にする。
struct Tuning {
    // ゲームの感度設定。1.0倍＝標準、最大20倍（キー・マウス・外部指令共通）。
    // 生マウス入力の変換率とは分け、実際の最大移動／旋回速度に適用する。
    double moveSensitivity = 1.0;
    double turnSensitivity = 1.0;
    double mouseHorizontal = (1.0 / 12.0); // 旧設定ファイルとの互換用。新UIでは非表示
    double mouseVertical = (1.0 / 12.0);   // 同上
    double mouseButtonStrength = 1.0;     // マウスボタンを走行軸に割当てたときの指令強度
    double bodySwayDegrees = 1.0;         // 加減速と旋回による揺れの最大規模
    double shotRecoilDegrees = 0.8;       // 発射時の射線とカメラの揺れ
    double turnInertiaMs = 450.0;       // 旋回角速度の追従・減衰時定数（ms）
    int panelPattern = 1;                // ヒットパネル中心高さのプリセット
    double tireSlip = 0.15;               // 0:高グリップ, 1:大きく滑る
    double commandDelayMs = 100;
    double videoDelayMs = 100;
    double displayFps = 20;

    double maxMoveSpeedCmPerSec() const { return 150.0 * moveSensitivity; }
    double maxTurnRateDegPerSec() const { return 120.0 * turnSensitivity; }

    bool valid() const {
        const double v[] = {moveSensitivity, turnSensitivity,
                            mouseHorizontal, mouseVertical, mouseButtonStrength,
                            bodySwayDegrees, shotRecoilDegrees, tireSlip, turnInertiaMs,
                            commandDelayMs, videoDelayMs, displayFps};
        for (double x : v) if (!std::isfinite(x)) return false;
        return moveSensitivity >= 0.1 && moveSensitivity <= 20.0
            && turnSensitivity >= 0.1 && turnSensitivity <= 20.0
            && mouseHorizontal >= 0.001 && mouseHorizontal <= 1.0
            && mouseVertical >= 0.001 && mouseVertical <= 1.0
            && mouseButtonStrength >= 0.05 && mouseButtonStrength <= 1.0
            && bodySwayDegrees >= 0 && bodySwayDegrees <= 8
            && shotRecoilDegrees >= 0 && shotRecoilDegrees <= 8
            && tireSlip >= 0 && tireSlip <= 1
            && turnInertiaMs >= 0 && turnInertiaMs <= 1500
            && panelPatternIsValid(panelPattern)
            && commandDelayMs >= 0 && commandDelayMs <= 1000
            && videoDelayMs >= 0 && videoDelayMs <= 1000
            && displayFps >= 1 && displayFps <= 60;
    }
};
} // namespace RobotSim
