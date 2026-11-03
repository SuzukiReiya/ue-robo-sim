#pragma once
#include "SimInputBindings.h"
class APlayerController;

// UE固有のキー・マウス状態を共通の生入力へ変換する。
// 指令・自動制御・機体運動の知識を持たせない。
class FSimKeyboardInput
{
public:
    static RobotSim::RawInput Read(const APlayerController* PC);
    static bool EmergencyStop(const APlayerController* PC);
};
