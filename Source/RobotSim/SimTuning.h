#pragma once
#include <cmath>

namespace RobotSim {
// 実験条件。大きい値が何を意味するかを各パラメータ名で明確にする。
struct Tuning {
    double mouseHorizontal = (1.0 / 12.0); // マウス左右の指令/px
    double mouseVertical = (1.0 / 12.0);   // マウス上下の指令/px
    double mouseButtonStrength = 1.0;     // マウスボタンを走行軸に割当てたときの指令強度
    double bodySwayDegrees = 1.0;         // 加減速と旋回による揺れの最大規模
    double shotRecoilDegrees = 0.8;       // 発射時の射線とカメラの揺れ
    double tireSlip = 0.15;               // 0:高グリップ, 1:大きく滑る
    double commandDelayMs = 100;
    double videoDelayMs = 100;
    double displayFps = 20;

    bool valid() const {
        const double v[] = {mouseHorizontal, mouseVertical, mouseButtonStrength,
                            bodySwayDegrees, shotRecoilDegrees, tireSlip,
                            commandDelayMs, videoDelayMs, displayFps};
        for (double x : v) if (!std::isfinite(x)) return false;
        return mouseHorizontal >= 0.001 && mouseHorizontal <= 1.0
            && mouseVertical >= 0.001 && mouseVertical <= 1.0
            && mouseButtonStrength >= 0.05 && mouseButtonStrength <= 1.0
            && bodySwayDegrees >= 0 && bodySwayDegrees <= 8
            && shotRecoilDegrees >= 0 && shotRecoilDegrees <= 8
            && tireSlip >= 0 && tireSlip <= 1
            && commandDelayMs >= 0 && commandDelayMs <= 1000
            && videoDelayMs >= 0 && videoDelayMs <= 1000
            && displayFps >= 1 && displayFps <= 60;
    }
};
} // namespace RobotSim
