#pragma once
#include "SimControl.h"
class APlayerController;

// UE固有のキー判定を、デバイス非依存の操作状態へ写すアダプタ。
// 共有制御・自動制御のロジックは含めない。
class FSimKeyboardInput
{
public:
    static RobotSim::OperatorButtons Read(const APlayerController* PC);
    static bool EmergencyStop(const APlayerController* PC);
};
