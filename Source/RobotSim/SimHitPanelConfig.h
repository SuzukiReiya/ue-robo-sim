#pragma once
#include <algorithm>
#include <cmath>

namespace RobotSim {
constexpr int PanelPatternCount = 5;
constexpr double PanelAngleDegrees = 75.0;
constexpr double PanelCenterLimitCm = 15.0;
constexpr double PanelSideLengthCm = 9.0;
constexpr double PanelRadialOffsetCm = 11.5;
// 各ロボットに前・右・後・左の4枚を剛体として固定する。
// パターン番号は設定JSONと検証ログで再現可能にする。
inline double panelHeightCm(int pattern, int side) {
    if (side < 0 || side >= 4) return 0;
    switch (pattern) {
    case 0: return 6.0;   // 低位置：下面でも地表面に触れない。
    case 1: return 9.0;   // 標準
    case 2: return 12.0;  // 高位置
    case 3: return 14.5;  // 上限150mmに0.5cmの余裕
    case 4: return side == 0 || side == 2 ? 12.0 : 6.0; // 前後高・左右低
    default: return 0;
    }
}
inline bool panelPatternIsValid(int pattern) {
    if (pattern < 0 || pattern >= PanelPatternCount) return false;
    for (int side=0;side<4;++side) {
        const double center = panelHeightCm(pattern,side);
        const double halfVertical = 0.5 * PanelSideLengthCm * std::sin(
            PanelAngleDegrees * 3.14159265358979323846 / 180.0);
        if (center > PanelCenterLimitCm || center-halfVertical < 0) return false;
    }
    return true;
}
} // namespace RobotSim
