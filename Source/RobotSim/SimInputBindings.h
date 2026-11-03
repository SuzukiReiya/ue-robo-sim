#pragma once
#include "SimCore.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace RobotSim {

// Escは設定画面、Spaceは非常停止、1/2/3とF1は研究操作の予約キー。
enum class InputSource : std::uint8_t {
    None, W, A, S, D, Q, E, R, F, Z, X, C, V, I, J, K, L,
    Shift, Ctrl, Up, Down, Left, Right,
    MouseUp, MouseDown, MouseLeft, MouseRight, MouseButtonLeft, MouseButtonRight,
    Count
};
constexpr std::size_t SourceCount = static_cast<std::size_t>(InputSource::Count);
inline std::size_t index(InputSource key) { return static_cast<std::size_t>(key); }

inline const char* sourceToken(InputSource key) {
    constexpr const char* Names[] = {
        "なし", "W", "A", "S", "D", "Q", "E", "R", "F",
        "Z", "X", "C", "V", "I", "J", "K", "L",
        "Shift", "Ctrl", "Up", "Down", "Left", "Right",
        "MouseUp", "MouseDown", "MouseLeft", "MouseRight", "MouseLMB", "MouseRMB"
    };
    const std::size_t n = index(key);
    return n < SourceCount ? Names[n] : Names[0];
}
inline bool parseSource(const char* token, InputSource& out) {
    if (!token) return false;
    for (std::size_t i=0; i<SourceCount; ++i) {
        const char* a = sourceToken(static_cast<InputSource>(i));
        const char* b = token;
        while (*a && *a == *b) { ++a; ++b; }
        if (!*a && !*b) { out = static_cast<InputSource>(i); return true; }
    }
    return false;
}
struct InputBindings {
    InputSource forward = InputSource::W;
    InputSource backward = InputSource::S;
    InputSource turnLeft = InputSource::A;
    InputSource turnRight = InputSource::D;
    InputSource fire = InputSource::MouseButtonLeft;

    bool valid() const {
        const std::array<InputSource, 5> a = {forward, backward, turnLeft, turnRight, fire};
        for (std::size_t i=0; i<a.size(); ++i) {
            if (index(a[i]) >= SourceCount) return false;
            if (i < 4 && a[i] == InputSource::None) return false;
            if (a[i] == InputSource::None) continue;
            for (std::size_t j=i+1; j<a.size(); ++j)
                if (a[i] == a[j]) return false;
        }
        return true;
    }
};
struct RawInput {
    std::array<bool, SourceCount> held{};
    double mouseX = 0, mouseY = 0;
};
// マウスの移動量は1フレーム分のデルタ。向きと感度だけをここで扱い、制御モードには依存しない。
inline double sourceValue(InputSource source, const RawInput& raw) {
    if (source == InputSource::None || index(source) >= SourceCount) return 0;
    constexpr double Sensitivity = 1.0 / 12.0;
    switch (source) {
    case InputSource::MouseLeft: return std::clamp(-raw.mouseX * Sensitivity, 0.0, 1.0);
    case InputSource::MouseRight: return std::clamp(raw.mouseX * Sensitivity, 0.0, 1.0);
    case InputSource::MouseUp: return std::clamp(-raw.mouseY * Sensitivity, 0.0, 1.0);
    case InputSource::MouseDown: return std::clamp(raw.mouseY * Sensitivity, 0.0, 1.0);
    default: return raw.held[index(source)] ? 1.0 : 0.0;
    }
}
struct OperatorActions {
    Command movement;
    bool fire = false;
};
inline OperatorActions mapInput(const InputBindings& bindings, const RawInput& raw) {
    OperatorActions result;
    result.movement.throttle = std::clamp(
        sourceValue(bindings.forward, raw) - sourceValue(bindings.backward, raw), -1.0, 1.0);
    result.movement.turn = std::clamp(
        sourceValue(bindings.turnRight, raw) - sourceValue(bindings.turnLeft, raw), -1.0, 1.0);
    result.fire = sourceValue(bindings.fire, raw) > 0.2;
    return result;
}
} // namespace RobotSim
