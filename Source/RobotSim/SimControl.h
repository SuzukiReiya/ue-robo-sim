#pragma once
#include "SimCore.h"

namespace RobotSim {

// 入力デバイスの状態。WASD以外の操縦器にも置き換えられる。
struct OperatorButtons {
    bool forward = false, backward = false, left = false, right = false;
};
inline Command toCommand(OperatorButtons keys) {
    return {(keys.forward ? 1.0 : 0.0) - (keys.backward ? 1.0 : 0.0),
            (keys.right ? 1.0 : 0.0) - (keys.left ? 1.0 : 0.0)};
}

enum class Mode { Manual = 0, Shared = 1, Automatic = 2 };

// 操縦入力・制御指令の変換と裁定。UE・機体・カメラには依存しない。
// 将来の共有制御・自動制御は、この入力口またはresolve()を置き換える。
class CommandRouter {
    Mode mode_ = Mode::Manual;
    DelayQueue<Command> manualQueue_;
    Command manual_{}, external_{}, applied_{};
    double lastManual_ = -100.0, lastExternal_ = -100.0, lastSequence_ = -1.0;
public:
    void selectMode(Mode next) {
        mode_ = next;
        manualQueue_.clear();
        manual_ = {}; external_ = {}; applied_ = {};
        lastManual_ = -100.0;
        lastExternal_ = -100.0;
        // 送信元のseqはモード切替後も単調増加を要求する。
    }
    void submitButtons(OperatorButtons buttons, double now, double commandDelay) {
        manualQueue_.push(now, commandDelay, toCommand(buttons));
    }
    bool submitExternal(double now, double sequence, Command command) {
        if (!std::isfinite(sequence) || !std::isfinite(command.throttle) ||
            !std::isfinite(command.turn) || sequence <= lastSequence_) return false;
        lastSequence_ = sequence;
        lastExternal_ = now;
        external_ = {std::clamp(command.throttle, -1.0, 1.0),
                     std::clamp(command.turn, -1.0, 1.0)};
        return true;
    }
    Command resolve(double now, double sharedWeight) {
        if (manualQueue_.poll(now, manual_)) lastManual_ = now;
        if (now - lastManual_ > 0.25) manual_ = {};
        const bool fresh = now - lastExternal_ <= 0.25;
        const Command local = fresh ? external_ : Command{};
        switch (mode_) {
        case Mode::Manual: applied_ = manual_; break;
        case Mode::Shared: applied_ = fresh ? blend(manual_, local, sharedWeight) : manual_; break;
        case Mode::Automatic: applied_ = local; break;
        }
        return applied_;
    }
    void emergencyStop() {
        manualQueue_.clear();
        manual_ = {}; external_ = {}; applied_ = {};
        lastManual_ = -100.0; lastExternal_ = -100.0;
    }
    Mode mode() const { return mode_; }
    Command manual() const { return manual_; }
    Command applied() const { return applied_; }
    Command freshExternal(double now) const {
        return now - lastExternal_ <= 0.25 ? external_ : Command{};
    }
};
} // namespace RobotSim
